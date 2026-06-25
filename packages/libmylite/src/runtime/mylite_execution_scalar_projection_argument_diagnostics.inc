
static const char *do_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    const char *function_name = NULL;

    while (expression != NULL) {
        function_name = argument_count_error_function_name(expression);
        if (function_name != NULL) {
            return function_name;
        }
        expression = expression->next_sibling;
    }

    return NULL;
}

static const char *select_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const char *function_name = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return NULL;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        function_name = argument_count_error_function_name(expression);
        if (function_name != NULL) {
            return function_name;
        }
        select_item = select_item->next_sibling;
    }

    return NULL;
}

static const char *argument_count_error_function_name(
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    const char *function_name = NULL;

    if (expression == NULL) {
        return NULL;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return NULL;
    }
    while (stack.count != 0U && function_name == NULL) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        function_name = argument_count_error_node_function_name(current);
        if (function_name != NULL) {
            break;
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                scalar_arithmetic_node_stack_deinit(&stack);
                return NULL;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return function_name;
}

static const char *argument_count_error_node_function_name(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return NULL;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
        return "CONNECTION_ID";
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
        return "VERSION";
    case MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR:
        return "PI";
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
        return "RAND";
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
        return "SQRT";
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
        return "DEGREES";
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
        return "RADIANS";
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
        return "ACOS";
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
        return "ASIN";
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
        return "SIN";
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
        return "COS";
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
        return "TAN";
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
        return "COT";
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
        return "ATAN";
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
        return "ATAN2";
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
        return "EXP";
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
        return "LN";
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
        return "LOG10";
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
        return "LOG2";
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
        return "POW";
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
        return "POWER";
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
        return "ANY_VALUE";
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
        return "CURRENT_ROLE";
    case MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR:
        return "FOUND_ROWS";
    case MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR:
        return "LAST_INSERT_ID";
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        return "IFNULL";
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        return "NULLIF";
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        return "ISNULL";
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
        return "ABS";
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
        return "SIGN";
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
        return "CEIL";
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
        return "CEILING";
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
        return "FLOOR";
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
        return "ROUND";
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
        return "BIN";
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
        return "OCT";
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
        return "CONV";
    case MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR:
        return "CONCAT";
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
        return "CONCAT_WS";
    case MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR:
        return "ELT";
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
        return "EXPORT_SET";
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
        return "FIELD";
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
        return "MAKE_SET";
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
        return "GREATEST";
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
        return "LEAST";
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
        return "JSON_VALID";
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
        return "JSON_EXTRACT";
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
        return "JSON_CONTAINS";
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
        return "JSON_CONTAINS_PATH";
    case MYLITE_SQL_AST_JSON_OVERLAPS_ARGUMENT_COUNT_ERROR:
        return "JSON_OVERLAPS";
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
        return "JSON_LENGTH";
    case MYLITE_SQL_AST_JSON_DEPTH_ARGUMENT_COUNT_ERROR:
        return "JSON_DEPTH";
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_ARGUMENT_COUNT_ERROR:
        return "JSON_STORAGE_SIZE";
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_ARGUMENT_COUNT_ERROR:
        return "JSON_STORAGE_FREE";
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
        return "JSON_KEYS";
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
        return "JSON_TYPE";
    case MYLITE_SQL_AST_JSON_PRETTY_ARGUMENT_COUNT_ERROR:
        return "JSON_PRETTY";
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
        return "JSON_SET";
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_ARGUMENT_COUNT_ERROR:
        return "JSON_ARRAY_APPEND";
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_ARGUMENT_COUNT_ERROR:
        return "JSON_ARRAY_INSERT";
    case MYLITE_SQL_AST_JSON_MERGE_ARGUMENT_COUNT_ERROR:
        return "JSON_MERGE";
    case MYLITE_SQL_AST_JSON_MERGE_PATCH_ARGUMENT_COUNT_ERROR:
        return "JSON_MERGE_PATCH";
    case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_ARGUMENT_COUNT_ERROR:
        return "JSON_MERGE_PRESERVE";
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
        return "JSON_REPLACE";
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
        return "JSON_REMOVE";
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
        return "JSON_QUOTE";
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
        return "JSON_UNQUOTE";
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
        return "FIND_IN_SET";
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
        return "STRCMP";
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
        return "SUBSTRING_INDEX";
    case MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR:
        return "LPAD";
    case MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR:
        return "RPAD";
    case MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR:
        return "SPACE";
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
        return "REGEXP_LIKE";
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_INSTR";
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_SUBSTR";
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        return "REGEXP_REPLACE";
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
        return "SOUNDEX";
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
        return "DATE_FORMAT";
    case MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR:
        return "TIME_FORMAT";
    case MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR:
        return "STR_TO_DATE";
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
        return "DATEDIFF";
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
        return "TIMEDIFF";
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "TIMESTAMP";
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "UNIX_TIMESTAMP";
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
        return "TIME_TO_SEC";
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
        return "TO_DAYS";
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
        return "TO_SECONDS";
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
        return "SEC_TO_TIME";
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
        return "FROM_UNIXTIME";
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
        return "FROM_DAYS";
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
        return "MAKEDATE";
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
        return "MAKETIME";
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
        return "DAYNAME";
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
        return "DAYOFMONTH";
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
        return "DAYOFWEEK";
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
        return "DAYOFYEAR";
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
        return "LAST_DAY";
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
        return "MONTHNAME";
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
        return "WEEKDAY";
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
        return "WEEKOFYEAR";
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
        return "YEARWEEK";
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "CURRENT_TIMESTAMP";
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        return "SYSDATE";
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
        return "BIT_COUNT";
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
        return "CRC32";
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
        return "MD5";
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
        return "SHA";
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
        return "SHA1";
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
        return "SHA2";
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
        return "HEX";
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
        return "TO_BASE64";
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
        return "FROM_BASE64";
    case MYLITE_SQL_AST_COMPRESS_ARGUMENT_COUNT_ERROR:
        return "COMPRESS";
    case MYLITE_SQL_AST_UNCOMPRESS_ARGUMENT_COUNT_ERROR:
        return "UNCOMPRESS";
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_ARGUMENT_COUNT_ERROR:
        return "UNCOMPRESSED_LENGTH";
    case MYLITE_SQL_AST_RANDOM_BYTES_ARGUMENT_COUNT_ERROR:
        return "RANDOM_BYTES";
    case MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR:
        return "INET_ATON";
    case MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR:
        return "INET_NTOA";
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
        return "UNHEX";
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
        return "UUID";
    case MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR:
        return "UUID_SHORT";
    case MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR:
        return "IS_UUID";
    case MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR:
        return "UUID_TO_BIN";
    case MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR:
        return "BIN_TO_UUID";
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
        return "QUOTE";
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
        return "FORMAT";
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
        return "TRUNCATE";
    case MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR:
        return "LENGTH";
    case MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR:
        return "OCTET_LENGTH";
    case MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR:
        return "BIT_LENGTH";
    case MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR:
        return "CHAR_LENGTH";
    case MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR:
        return "CHARACTER_LENGTH";
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
        return "LOWER";
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
        return "LCASE";
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
        return "UPPER";
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
        return "UCASE";
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
        return "LTRIM";
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
        return "RTRIM";
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
        return "LOCATE";
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
        return "INSTR";
    default:
        break;
    }
    return NULL;
}
