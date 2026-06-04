static int append_row_scalar_json_valid_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_valid(");
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

static int append_row_scalar_json_search_expression_sql(
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
            rc = append_row_scalar_json_introspection_argument_sql(
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

static int append_row_scalar_json_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_extract(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_extract_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_value_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_value(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_extract_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_json_unquote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_unquote(");
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

static int append_row_scalar_json_quote_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_quote(");
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

static int append_row_scalar_json_unquote_extract_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_unquote(_mylite_json_extract(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_extract_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }
    return rc;
}

static int append_row_scalar_json_extract_document_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return append_row_scalar_json_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_json_introspection_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
        return append_row_scalar_json_set_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        return append_row_scalar_json_extract_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_json_length_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_length(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_introspection_argument_sql(
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

static int append_row_scalar_json_keys_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 1U && expression->argument_count != 2U)) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_keys(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_introspection_argument_sql(
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

static int append_row_scalar_json_type_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "_mylite_json_type(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_introspection_argument_sql(
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

static int append_row_scalar_json_set_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const char *sqlite_function_name = NULL;
    int rc = MYLITE_OK;

    rc = json_mutation_sqlite_function_name(expression, &sqlite_function_name);
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, sqlite_function_name);
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_json_set_document_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        if (expression->json_mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
            rc = append_row_scalar_json_remove_arguments_sql(string, expression, next_parameter);
        } else {
            rc = append_row_scalar_json_set_pair_arguments_sql(string, expression, next_parameter);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int json_mutation_sqlite_function_name(
    const struct planned_row_scalar_expression *expression,
    const char **out_function_name
) {
    if (expression == NULL || expression->arguments == NULL || out_function_name == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->json_mutation_kind) {
    case PLANNED_JSON_MUTATION_SET:
        if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_set(";
        return MYLITE_OK;
    case PLANNED_JSON_MUTATION_INSERT:
        if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_insert(";
        return MYLITE_OK;
    case PLANNED_JSON_MUTATION_REPLACE:
        if (expression->argument_count < 3U || (expression->argument_count % 2U) == 0U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_replace(";
        return MYLITE_OK;
    case PLANNED_JSON_MUTATION_REMOVE:
        if (expression->argument_count < 2U) {
            return MYLITE_ERROR;
        }
        *out_function_name = "_mylite_json_remove(";
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_json_remove_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
    }
    return rc;
}

static int append_row_scalar_json_set_pair_arguments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         argument_index += 2U) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_set_value_argument_sql(
                string,
                &expression->arguments[argument_index + 1U],
                next_parameter
            );
        }
    }
    return rc;
}

static int append_row_scalar_json_set_document_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        break;
    }

    return MYLITE_ERROR;
}

static int append_row_scalar_json_set_value_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
        rc = append_row_scalar_json_value_kind_sql(string, MYLITE_JSON_SQL_VALUE_JSON);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_constructor_expression_sql(
                string,
                "_mylite_json_array",
                argument,
                next_parameter
            );
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        rc = append_row_scalar_json_value_kind_sql(string, MYLITE_JSON_SQL_VALUE_JSON);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_json_constructor_expression_sql(
                string,
                "_mylite_json_object",
                argument,
                next_parameter
            );
        }
        break;
    default:
        rc = append_row_scalar_json_constructor_argument_sql(string, argument, next_parameter);
        break;
    }
    return rc;
}

static int append_row_scalar_json_value_kind_sql(
    struct mylite_dynamic_string *string,
    enum mylite_json_sql_value_kind value_kind
) {
    char tag[integer_text_capacity];
    int written = snprintf(tag, sizeof(tag), "%d", (int)value_kind);

    if (written < 0 || (size_t)written >= sizeof(tag)) {
        return MYLITE_ERROR;
    }

    return mylite_dynamic_string_append(string, tag);
}

static int append_row_scalar_json_constructor_expression_sql(
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
            rc = append_row_scalar_json_constructor_argument_sql(
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

static int append_row_scalar_json_constructor_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (argument == NULL) {
        return MYLITE_ERROR;
    }

    rc = append_row_scalar_json_value_kind_sql(string, argument->json_value_kind);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
    }
    return rc;
}

static int append_row_scalar_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
        return append_row_scalar_if_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
        return append_row_scalar_ifnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
        return append_row_scalar_coalesce_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        return append_row_scalar_nullif_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return append_row_scalar_isnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        return append_row_scalar_searched_case_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        return append_row_scalar_like_predicate_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_nested_control_flow_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
        return append_row_scalar_nested_if_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
        return append_row_scalar_nested_ifnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
        return append_row_scalar_nested_coalesce_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        return append_row_scalar_nested_nullif_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
        return append_row_scalar_nested_isnull_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        return append_row_scalar_nested_searched_case_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        return append_row_scalar_leaf_like_predicate_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_if_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN COALESCE(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 0) <> 0 THEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[2],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_ifnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "ifnull(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
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

static int append_row_scalar_coalesce_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "coalesce(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
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

static int append_row_scalar_nullif_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    bool use_string_collation =
        (expression != NULL &&
         expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) != 0;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "nullif(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK && use_string_collation) {
            rc = append_row_scalar_control_flow_collated_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
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

static int append_row_scalar_isnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL)");
    }
    return rc;
}

static int append_row_scalar_searched_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    size_t result_argument_limit = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }
    if ((expression->argument_count % 2U) == 0U) {
        result_argument_limit = expression->argument_count;
    } else if (expression->case_has_else) {
        result_argument_limit = expression->argument_count - 1U;
    } else {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < result_argument_limit;
         argument_index += 2U) {
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
                string,
                &expression->arguments[argument_index + 1U],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK && expression->case_has_else) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_argument_sql(
                string,
                &expression->arguments[expression->argument_count - 1U],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_nested_searched_case_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    size_t result_argument_limit = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }
    if ((expression->argument_count % 2U) == 0U) {
        result_argument_limit = expression->argument_count;
    } else if (expression->case_has_else) {
        result_argument_limit = expression->argument_count - 1U;
    } else {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < result_argument_limit;
         argument_index += 2U) {
        rc = mylite_dynamic_string_append(string, " WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_condition_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " THEN ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
                string,
                &expression->arguments[argument_index + 1U],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK && expression->case_has_else) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
                string,
                &expression->arguments[expression->argument_count - 1U],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_like_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE ||
        expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    operator_kind = expression->arithmetic_operator;

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK && (operator_kind == MYLITE_SQL_AST_OPERATOR_NONE ||
                            operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE)) {
        rc = mylite_dynamic_string_append(string, " LIKE ");
    } else if (
        rc == MYLITE_OK && row_scalar_control_flow_comparison_operator_is_supported(operator_kind)
    ) {
        rc = mylite_dynamic_string_append_char(string, ' ');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, comparison_operator_sql(operator_kind));
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
    } else if (rc == MYLITE_OK) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_argument_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK && operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE &&
        expression->like_uses_escape) {
        rc = mylite_dynamic_string_append(string, " ESCAPE '\\'");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_leaf_like_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE ||
        expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    operator_kind = expression->arithmetic_operator;

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK && (operator_kind == MYLITE_SQL_AST_OPERATOR_NONE ||
                            operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE)) {
        rc = mylite_dynamic_string_append(string, " LIKE ");
    } else if (
        rc == MYLITE_OK && row_scalar_control_flow_comparison_operator_is_supported(operator_kind)
    ) {
        rc = mylite_dynamic_string_append_char(string, ' ');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, comparison_operator_sql(operator_kind));
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ' ');
        }
    } else if (rc == MYLITE_OK) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK && operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE &&
        expression->like_uses_escape) {
        rc = mylite_dynamic_string_append(string, " ESCAPE '\\'");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_nested_if_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN COALESCE(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", 0) <> 0 THEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[1],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ELSE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[2],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " END)");
    }
    return rc;
}

static int append_row_scalar_nested_ifnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "ifnull(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
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

static int append_row_scalar_nested_coalesce_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count == 0U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "coalesce(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
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

static int append_row_scalar_nested_nullif_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    bool use_string_collation =
        (expression != NULL &&
         expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) != 0;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(string, "nullif(");
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        if (argument_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK && use_string_collation) {
            rc = append_row_scalar_control_flow_leaf_collated_argument_sql(
                string,
                &expression->arguments[argument_index],
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_row_scalar_control_flow_leaf_argument_sql(
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

static int append_row_scalar_nested_isnull_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_control_flow_leaf_argument_sql(
            string,
            &expression->arguments[0],
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL)");
    }
    return rc;
}

static int append_row_scalar_control_flow_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        return append_row_scalar_nested_control_flow_expression_sql(
            string,
            expression,
            next_parameter
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_control_flow_collated_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = append_row_scalar_control_flow_argument_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK) {
        rc = append_string_key_collation_sql(string);
    }
    return rc;
}

static int append_row_scalar_control_flow_leaf_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_control_flow_leaf_condition_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    if (expression == NULL) {
        return MYLITE_ERROR;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return append_row_scalar_non_control_flow_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        return append_row_scalar_leaf_like_predicate_expression_sql(string, expression, next_parameter);
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
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
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
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
        break;
    }
    return MYLITE_ERROR;
}

static int append_row_scalar_control_flow_leaf_collated_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = append_row_scalar_control_flow_leaf_argument_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK) {
        rc = append_string_key_collation_sql(string);
    }
    return rc;
}

static int append_row_scalar_left_right_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    bool use_right = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_LEFT &&
        expression->string_slice_kind != PLANNED_STRING_SLICE_FUNCTION_RIGHT) {
        return MYLITE_ERROR;
    }

    value = &expression->arguments[0];
    length = &expression->arguments[1];
    use_right = expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_RIGHT;

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL WHEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <= 0 THEN '' ELSE substr(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK && use_right) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, true);
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "1, ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") END)");
    }
    return rc;
}

static int append_row_scalar_substring_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *value = NULL;
    const struct planned_row_scalar_expression *position = NULL;
    const struct planned_row_scalar_expression *length = NULL;
    bool has_length = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }

    value = &expression->arguments[0];
    position = &expression->arguments[1];
    has_length = expression->argument_count == 3U;
    if (has_length) {
        length = &expression->arguments[2];
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_substring_null_guard_sql(
            string,
            value,
            position,
            length,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " THEN NULL WHEN ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = 0 THEN ''");
    }
    if (rc == MYLITE_OK && has_length) {
        rc = append_row_scalar_substring_nonpositive_length_guard_sql(
            string,
            length,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_substring_negative_start_guard_sql(
            string,
            value,
            position,
            next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_substring_call_sql(string, value, position, length, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") END)");
    }
    return rc;
}

static int append_row_scalar_substring_null_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL");
    }
    if (rc == MYLITE_OK && length != NULL) {
        rc = mylite_dynamic_string_append(string, " OR ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NULL");
        }
    }
    return rc;
}

static int append_row_scalar_substring_nonpositive_length_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = mylite_dynamic_string_append(string, " WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <= 0 THEN ''");
    }
    return rc;
}

static int append_row_scalar_substring_negative_start_guard_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = mylite_dynamic_string_append(string, " WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " < 0 AND length(");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") + ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " < 0 THEN '' ELSE ");
    }
    return rc;
}

static int append_row_scalar_substring_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value,
    const struct planned_row_scalar_expression *position,
    const struct planned_row_scalar_expression *length,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    rc = mylite_dynamic_string_append(string, "substr(");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, value, next_parameter, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_string_slice_operand_sql(string, position, next_parameter, false);
    }
    if (rc == MYLITE_OK && length != NULL) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_string_slice_operand_sql(string, length, next_parameter, false);
        }
    }
    return rc;
}

static int append_row_scalar_string_slice_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter,
    bool negate
) {
    int rc = MYLITE_OK;

    if (negate) {
        rc = mylite_dynamic_string_append(string, "(-");
    }
    if (rc == MYLITE_OK && expression != NULL &&
        expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC) {
        rc = append_row_scalar_integer_arithmetic_expression_sql(
            string,
            expression,
            next_parameter
        );
    } else if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, expression, next_parameter);
    }
    if (rc == MYLITE_OK && negate) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_row_scalar_hex_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    const struct planned_row_scalar_expression *argument = NULL;
    bool argument_is_integer = false;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 1U || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    argument = &expression->arguments[0];
    if (argument->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        if (strcmp(argument->column.physical_type, "INTEGER") == 0) {
            argument_is_integer = true;
        }
    }

    if (!argument_is_integer) {
        rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL ELSE hex(");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ") END)");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append(string, "(CASE WHEN ");
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL THEN NULL ELSE printf('%llX', ");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_non_concat_expression_sql(string, argument, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") END)");
    }
    return rc;
}
