static int append_row_scalar_date_format_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE) {
        rc = mylite_dynamic_string_append(string, "_mylite_str_to_date(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT) {
        rc = mylite_dynamic_string_append(string, "_mylite_time_format(");
    } else {
        rc = mylite_dynamic_string_append(string, "_mylite_date_format(");
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_unhex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_unhex(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_base64_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64) {
        rc = mylite_dynamic_string_append(string, "_mylite_to_base64(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64) {
        rc = mylite_dynamic_string_append(string, "_mylite_from_base64(");
    } else {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

// Registered scalar functions delegate argument rendering back to the general row-scalar walker.
// NOLINTNEXTLINE(misc-no-recursion)
static int append_row_scalar_registered_function_expression_sql(
    struct mylite_dynamic_string *string,
    const char *function_name,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (function_name == NULL || expression == NULL ||
        (expression->argument_count != 0U && expression->arguments == NULL)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_numeric_function_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION) {
        return MYLITE_ERROR;
    }
    function_name = row_scalar_numeric_function_sql_name(expression->numeric_function_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    return append_row_scalar_registered_function_expression_sql(
        string,
        function_name,
        expression,
        next_parameter
    );
}

static const char *row_scalar_numeric_function_sql_name(
    enum planned_row_scalar_numeric_function_kind kind
) {
    switch (kind) {
    case PLANNED_ROW_SCALAR_NUMERIC_ABS:
        return "_mylite_numeric_abs";
    case PLANNED_ROW_SCALAR_NUMERIC_SIGN:
        return "_mylite_numeric_sign";
    case PLANNED_ROW_SCALAR_NUMERIC_CEIL:
        return "_mylite_numeric_ceil";
    case PLANNED_ROW_SCALAR_NUMERIC_CEILING:
        return "_mylite_numeric_ceiling";
    case PLANNED_ROW_SCALAR_NUMERIC_FLOOR:
        return "_mylite_numeric_floor";
    case PLANNED_ROW_SCALAR_NUMERIC_ROUND:
        return "_mylite_numeric_round";
    case PLANNED_ROW_SCALAR_NUMERIC_SQRT:
        return "_mylite_numeric_sqrt";
    case PLANNED_ROW_SCALAR_NUMERIC_DEGREES:
        return "_mylite_numeric_degrees";
    case PLANNED_ROW_SCALAR_NUMERIC_RADIANS:
        return "_mylite_numeric_radians";
    case PLANNED_ROW_SCALAR_NUMERIC_ACOS:
        return "_mylite_numeric_acos";
    case PLANNED_ROW_SCALAR_NUMERIC_ASIN:
        return "_mylite_numeric_asin";
    case PLANNED_ROW_SCALAR_NUMERIC_ATAN:
        return "_mylite_numeric_atan";
    case PLANNED_ROW_SCALAR_NUMERIC_ATAN2:
        return "_mylite_numeric_atan2";
    case PLANNED_ROW_SCALAR_NUMERIC_SIN:
        return "_mylite_numeric_sin";
    case PLANNED_ROW_SCALAR_NUMERIC_COS:
        return "_mylite_numeric_cos";
    case PLANNED_ROW_SCALAR_NUMERIC_TAN:
        return "_mylite_numeric_tan";
    case PLANNED_ROW_SCALAR_NUMERIC_COT:
        return "_mylite_numeric_cot";
    case PLANNED_ROW_SCALAR_NUMERIC_EXP:
        return "_mylite_numeric_exp";
    case PLANNED_ROW_SCALAR_NUMERIC_LN:
        return "_mylite_numeric_ln";
    case PLANNED_ROW_SCALAR_NUMERIC_LOG:
        return "_mylite_numeric_log";
    case PLANNED_ROW_SCALAR_NUMERIC_LOG10:
        return "_mylite_numeric_log10";
    case PLANNED_ROW_SCALAR_NUMERIC_LOG2:
        return "_mylite_numeric_log2";
    case PLANNED_ROW_SCALAR_NUMERIC_POW:
        return "_mylite_numeric_pow";
    case PLANNED_ROW_SCALAR_NUMERIC_POWER:
        return "_mylite_numeric_power";
    case PLANNED_ROW_SCALAR_NUMERIC_BIT_COUNT:
        return "_mylite_numeric_bit_count";
    case PLANNED_ROW_SCALAR_NUMERIC_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_bitwise_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_BITWISE) {
        return MYLITE_ERROR;
    }
    function_name = row_scalar_bitwise_sql_name(expression);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    return append_row_scalar_registered_function_expression_sql(
        string,
        function_name,
        expression,
        next_parameter
    );
}

static const char *row_scalar_bitwise_sql_name(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return NULL;
    }
    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        return "_mylite_bitwise_not";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        return "_mylite_bitwise_and";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        return "_mylite_bitwise_or";
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        return "_mylite_bitwise_xor";
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
        return "_mylite_bitwise_lshift";
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
        return "_mylite_bitwise_rshift";
    default:
        return NULL;
    }
}

static int append_row_scalar_base_conversion_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION) {
        return MYLITE_ERROR;
    }
    function_name = row_scalar_base_conversion_sql_name(expression->base_conversion_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    return append_row_scalar_registered_function_expression_sql(
        string,
        function_name,
        expression,
        next_parameter
    );
}

static const char *row_scalar_base_conversion_sql_name(
    enum planned_row_scalar_base_conversion_kind kind
) {
    switch (kind) {
    case PLANNED_ROW_SCALAR_BASE_CONVERSION_BIN:
        return "_mylite_bin";
    case PLANNED_ROW_SCALAR_BASE_CONVERSION_OCT:
        return "_mylite_oct";
    case PLANNED_ROW_SCALAR_BASE_CONVERSION_CONV:
        return "_mylite_conv";
    case PLANNED_ROW_SCALAR_BASE_CONVERSION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_numeric_extra_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA) {
        return MYLITE_ERROR;
    }
    if (expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE &&
        expression->numeric_extra_has_decimal_scale) {
        if (expression->argument_count != 2U || expression->arguments == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append(string, "_mylite_truncate_scaled(");
        for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
            if (argument_index != 0U) {
                rc = mylite_dynamic_string_append(string, ", ");
            }
            if (rc == MYLITE_OK) {
                rc = append_row_scalar_non_concat_expression_sql(
                    string,
                    &expression->arguments[argument_index],
                    next_parameter
                );
            }
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_size_literal(string, expression->numeric_extra_decimal_scale);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        return rc;
    }

    function_name = row_scalar_numeric_extra_sql_name(expression->numeric_extra_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    return append_row_scalar_registered_function_expression_sql(
        string,
        function_name,
        expression,
        next_parameter
    );
}

static const char *row_scalar_numeric_extra_sql_name(enum planned_row_scalar_numeric_extra_kind kind
) {
    switch (kind) {
    case PLANNED_ROW_SCALAR_NUMERIC_EXTRA_CRC32:
        return "_mylite_crc32";
    case PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT:
        return "_mylite_format";
    case PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE:
        return "_mylite_truncate";
    case PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI:
        return "_mylite_pi";
    case PLANNED_ROW_SCALAR_NUMERIC_EXTRA_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_uuid_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name =
        expression == NULL ? NULL : row_scalar_uuid_sql_function_name(expression->kind);
    int rc = MYLITE_OK;

    if (function_name == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_uuid_argument_sql(
                string,
                &expression->arguments[argument_index],
                argument_index,
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_uuid_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    size_t *next_parameter
) {
    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return append_row_scalar_uuid_leaf_argument_sql(string, argument, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        if (argument_index == 0U) {
            return append_row_scalar_uuid_inner_expression_sql(string, argument, next_parameter);
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_uuid_inner_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name =
        expression == NULL ? NULL : row_scalar_uuid_sql_function_name(expression->kind);
    int rc = MYLITE_OK;

    if (function_name == NULL || expression->argument_count > 2U ||
        (expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID &&
         expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT &&
         (expression->argument_count == 0U || expression->arguments == NULL))) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_uuid_leaf_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_uuid_leaf_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
        return mylite_dynamic_string_append(string, "NULL");
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return mylite_dynamic_string_append_quoted_identifier(string, argument->column->name);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        break;
    }
    return MYLITE_ERROR;
}

static const char *row_scalar_uuid_sql_function_name(enum planned_row_scalar_expression_kind kind) {
    switch (kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
        return "_mylite_uuid";
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
        return "_mylite_uuid_short";
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
        return "_mylite_is_uuid";
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
        return "_mylite_uuid_to_bin";
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        return "_mylite_bin_to_uuid";
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
        return NULL;
    }
    return NULL;
}

static int append_row_scalar_char_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_char(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK &&
            expression->arguments[argument_index].kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
            rc = append_numbered_parameter(string, *next_parameter);
            if (rc == MYLITE_OK) {
                ++(*next_parameter);
            }
        } else if (rc == MYLITE_OK && expression->arguments[argument_index].kind ==
                                          PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
            rc = append_row_scalar_column_sql(string, &expression->arguments[argument_index]);
        } else if (rc == MYLITE_OK) {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_date_format_numeric_comparison_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *operator_sql = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    operator_sql = comparison_operator_sql(expression->operator_kind);
    if (operator_sql == NULL || operator_sql[0] == '\0' ||
        expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return MYLITE_ERROR;
    }
    rc = append_row_scalar_date_format_numeric_operand_sql(
        string,
        &expression->arguments[0],
        next_parameter
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, operator_sql);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_date_format_numeric_operand_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    return rc;
}

static int append_row_scalar_date_format_numeric_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "CAST(");

    if (rc == MYLITE_OK) {
        if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT) {
            rc = append_row_scalar_date_format_expression_sql(string, expression, next_parameter);
        } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
            rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
        } else {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS REAL)");
    }
    return rc;
}

static int append_row_scalar_date_interval_second_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != date_interval_planned_argument_count ||
        expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "_mylite_date_interval_second(");
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < date_interval_planned_argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_datediff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_datediff(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_timediff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_timediff(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_addtime_subtime_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME) {
        function_name = "_mylite_addtime";
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME) {
        function_name = "_mylite_subtime";
    } else {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_timestampdiff_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        expression->argument_count != planned_row_scalar_timestampdiff_argument_count ||
        expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_timestampdiff(");
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < planned_row_scalar_timestampdiff_argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_field_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool use_string_collation
) {
    int rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK && use_string_collation) {
        rc = append_string_key_collation_sql(string);
    }
    return rc;
}

static int append_row_scalar_string_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *argument = NULL;
    bool argument_is_binary = false;
    bool force_blob = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    argument = &expression->arguments[0];
    argument_is_binary = row_scalar_string_length_argument_is_binary(argument);
    if (!argument_is_binary &&
        expression->string_length_kind != PLANNED_STRING_LENGTH_FUNCTION_CHARACTER) {
        force_blob = true;
    }

    if (expression->string_length_kind == PLANNED_STRING_LENGTH_FUNCTION_BIT) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_length_operand_sql(
            string,
            argument,
            next_parameter,
            force_blob
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK && expression->string_length_kind == PLANNED_STRING_LENGTH_FUNCTION_BIT) {
        rc = mylite_dynamic_string_append(string, " * 8)");
    }
    return rc;
}

static int append_row_scalar_string_length_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool force_blob
) {
    int rc = MYLITE_OK;

    if (force_blob) {
        rc = mylite_dynamic_string_append(string, "CAST(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    }
    if (rc == MYLITE_OK && force_blob) {
        rc = mylite_dynamic_string_append(string, " AS BLOB)");
    }
    return rc;
}

static bool row_scalar_string_length_argument_is_binary(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return false;
    }
    return mylite_execution_column_descriptor_is_binary_value(expression->column);
}

static int append_row_scalar_string_codepoint_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_codepoint_sql_function(expression->string_codepoint_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_codepoint_sql_function(
    enum planned_string_codepoint_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_CODEPOINT_FUNCTION_ASCII:
        return "_mylite_ascii";
    case PLANNED_STRING_CODEPOINT_FUNCTION_ORD:
        return "_mylite_ord";
    case PLANNED_STRING_CODEPOINT_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_case_sql_function(expression->string_case_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_case_sql_function(
    enum planned_string_case_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_CASE_FUNCTION_LOWER:
        return "_mylite_lower_ascii";
    case PLANNED_STRING_CASE_FUNCTION_UPPER:
        return "_mylite_upper_ascii";
    case PLANNED_STRING_CASE_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_trim_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_trim_sql_function(expression->string_trim_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_trim_sql_function(
    enum planned_string_trim_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_TRIM_FUNCTION_BOTH:
        return "_mylite_trim";
    case PLANNED_STRING_TRIM_FUNCTION_LEADING:
        return "_mylite_ltrim";
    case PLANNED_STRING_TRIM_FUNCTION_TRAILING:
        return "_mylite_rtrim";
    case PLANNED_STRING_TRIM_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_unix_timestamp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_unix_timestamp(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_temporal_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_temporal_extract(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_sec_to_time_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_sec_to_time(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_timestamp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "_mylite_timestamp(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_from_unixtime_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_from_unixtime(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_temporal_constructor_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
        function_name = "_mylite_from_days";
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
        function_name = "_mylite_makedate";
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
        function_name = "_mylite_maketime";
        break;
    default:
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_slice_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return append_row_scalar_substring_expression_sql(string, expression, next_parameter);
    }
    return append_row_scalar_left_right_expression_sql(string, expression, next_parameter);
}

static int append_row_scalar_string_padding_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_SPACE) {
        if (expression->argument_count != 1U) {
            return MYLITE_ERROR;
        }
    } else if (expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_REPEAT) {
        if (expression->argument_count != 2U) {
            return MYLITE_ERROR;
        }
    } else if (expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ||
               expression->string_padding_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD) {
        if (expression->argument_count != 3U) {
            return MYLITE_ERROR;
        }
    } else {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_padding_sql_function(expression->string_padding_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_padding_sql_function(
    enum planned_string_padding_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_PADDING_FUNCTION_LPAD:
        return "_mylite_lpad";
    case PLANNED_STRING_PADDING_FUNCTION_RPAD:
        return "_mylite_rpad";
    case PLANNED_STRING_PADDING_FUNCTION_REPEAT:
        return "_mylite_repeat";
    case PLANNED_STRING_PADDING_FUNCTION_SPACE:
        return "_mylite_space";
    case PLANNED_STRING_PADDING_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_bitmask_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if ((expression->string_bitmask_kind == PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET &&
         (expression->argument_count < string_bitmask_export_set_min_argument_count ||
          expression->argument_count > string_bitmask_export_set_max_argument_count)) ||
        (expression->string_bitmask_kind == PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET &&
         expression->argument_count < string_bitmask_make_set_min_argument_count)) {
        return MYLITE_ERROR;
    }

    function_name = row_scalar_string_bitmask_sql_function(expression->string_bitmask_kind);
    if (function_name == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, function_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static const char *row_scalar_string_bitmask_sql_function(
    enum planned_string_bitmask_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET:
        return "_mylite_export_set";
    case PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET:
        return "_mylite_make_set";
    case PLANNED_STRING_BITMASK_FUNCTION_NONE:
        break;
    }
    return NULL;
}

static int append_row_scalar_string_search_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U ||
        expression->string_search_kind == PLANNED_STRING_SEARCH_FUNCTION_NONE) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_locate_ascii_ci(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_replace_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_replace(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_insert_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_insert_string(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_reverse_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_reverse_utf8(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_soundex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_soundex(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_string_quote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_quote_sql(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_substring_index_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_substring_index(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_find_in_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_find_in_set_ascii_ci(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_strcmp_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_strcmp_ascii_ci(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_regexp_like_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    if (expression->regexp_case_sensitive) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_cs_ascii(");
    } else {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_ci_ascii(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_regexp_string_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }

    if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_instr_ci_ascii(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_substr_ci_ascii(");
    } else if (expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = mylite_dynamic_string_append(string, "_mylite_regexp_replace_ci_ascii(");
    } else {
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[2],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}
