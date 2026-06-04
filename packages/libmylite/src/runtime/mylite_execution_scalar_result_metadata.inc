
static int populate_scalar_binary_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct mylite_result_column_descriptor *descriptor
);
static int scalar_binary_literal_byte_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    size_t *out_byte_count
);

static int make_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;
    int rc = MYLITE_OK;

    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    unwrapped = unwrap_any_value_function_expression(expression);
    if (unwrapped == NULL) {
        return MYLITE_OK;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(unwrapped);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            const struct mylite_sql_ast_node *operand =
                unwrap_parenthesized_expression(child_at(unwrapped, 0U));

            if (operand != NULL && operand->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(operand) == MYLITE_SQL_AST_LITERAL_INTEGER) {
                return populate_scalar_literal_result_column_descriptor(
                    database,
                    unwrapped,
                    out_descriptor
                );
            }
        }
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        return populate_scalar_literal_result_column_descriptor(
            database,
            unwrapped,
            out_descriptor
        );
    }

    rc = populate_scalar_function_result_column_descriptor(database, unwrapped, out_descriptor);
    return rc;
}

static int populate_scalar_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
        if (literal == NULL) {
            return MYLITE_OK;
        }
    }
    if (literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        struct scalar_binary_numeric_result_column_shape shape = {0};
        int rc = scalar_integer_literal_result_column_shape(expression, &shape);

        if (rc != MYLITE_OK) {
            return MYLITE_OK;
        }
        populate_scalar_binary_numeric_result_column_descriptor(descriptor, shape);
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_tinyint_bool_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NULL,
                .display_length = 0U,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return populate_scalar_string_literal_result_column_descriptor(
            database,
            literal,
            descriptor
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
        literal_kind == MYLITE_SQL_AST_LITERAL_BIT) {
        return populate_scalar_binary_literal_result_column_descriptor(
            database,
            literal,
            descriptor
        );
    }

    return MYLITE_OK;
}

static int populate_scalar_function_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_database_function_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_user_function_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_version_function_display_length,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_regexp_string_function_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_UUID_FUNCTION:
        populate_uuid_string_result_column_descriptor(descriptor, true);
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return populate_scalar_charset_collation_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = MYLITE_UUID_BINARY_SIZE;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = MYLITE_UUID_TEXT_SIZE,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_type_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_value_display_character_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        populate_scalar_json_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        populate_scalar_datetime_result_column_descriptor(descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
        populate_calendar_name_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    default:
        return MYLITE_OK;
    }
}

static void populate_scalar_temporal_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    descriptor->charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = mysql_temporal_string_function_display_length *
                                 scalar_connection_max_bytes_per_character(database);
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = 0U;
    descriptor->nullable = true;
}

static void populate_calendar_name_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
) {
    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = mysql_calendar_name_function_display_length *
                              scalar_connection_max_bytes_per_character(database),
            .extra_flags = 0U,
            .nullable = true,
        }
    );
}

static void populate_scalar_date_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATE;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = date_text_length;
    descriptor->decimals = 0U;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
}

static void populate_scalar_datetime_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATETIME;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = datetime_text_length;
    descriptor->decimals = 0U;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
}

static int populate_scalar_string_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct mylite_result_column_descriptor *descriptor
) {
    char *text = NULL;
    size_t text_length = 0U;
    uint64_t max_bytes_per_character = scalar_connection_max_bytes_per_character(database);
    uint64_t display_length = 0U;
    int rc = decode_sql_string_literal_with_policy(
        database,
        literal,
        "scalar metadata supports only ordinary string literals",
        "scalar metadata does not support NUL bytes in string literals",
        true,
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    free(text);
    if (text_length > UINT64_MAX / max_bytes_per_character) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    display_length = (uint64_t)text_length * max_bytes_per_character;
    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = false,
        }
    );
    return MYLITE_OK;
}

static int populate_scalar_binary_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct mylite_result_column_descriptor *descriptor
) {
    size_t byte_count = 0U;
    int rc = scalar_binary_literal_byte_count(database, literal, &byte_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = (uint64_t)byte_count;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    descriptor->nullable = false;
    return MYLITE_OK;
}

static int scalar_binary_literal_byte_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    size_t *out_byte_count
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    const struct mylite_sql_source_span *span = NULL;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL || out_byte_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_byte_count = 0U;
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    span = &literal->span;
    if (span->text == NULL || span->length < 3U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        if (span->text[0] == '0' && (span->text[1] == 'x' || span->text[1] == 'X')) {
            *out_byte_count = (span->length - 1U) / 2U;
            return MYLITE_OK;
        }
        *out_byte_count = (span->length - 3U) / 2U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_BIT) {
        size_t bit_count = 0U;

        if ((span->text[0] == 'b' || span->text[0] == 'B') && span->text[1] == '\'' &&
            span->text[span->length - 1U] == '\'') {
            bit_count = span->length - 3U;
        } else if (
            span->text[0] == '0' && (span->text[1] == 'b' || span->text[1] == 'B')
        ) {
            bit_count = span->length - 2U;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        *out_byte_count = (bit_count + (size_t)CHAR_BIT - 1U) / (size_t)CHAR_BIT;
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int scalar_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *expression,
    struct scalar_binary_numeric_result_column_shape *out_shape
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(expression);
    bool is_negative = false;

    if (unwrapped == NULL || out_shape == NULL) {
        return MYLITE_MISUSE;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(unwrapped);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        unwrapped = unwrap_parenthesized_expression(child_at(unwrapped, 0U));
    }
    return scalar_unsigned_integer_literal_result_column_shape(unwrapped, is_negative, out_shape);
}

static int scalar_unsigned_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_binary_numeric_result_column_shape *out_shape
) {
    uint64_t magnitude = 0U;
    uint64_t display_length = 0U;
    uint64_t signed_min_magnitude = (uint64_t)INT64_MAX + 1U;

    if (out_shape == NULL || literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) == MYLITE_OK) {
        if (is_negative && magnitude > signed_min_magnitude) {
            return scalar_newdecimal_integer_literal_result_column_shape(&literal->span, out_shape);
        }
        display_length = (uint64_t)literal->span.length + 1U;
        if (display_length > mysql_bigint_display_length) {
            display_length = mysql_bigint_display_length;
        }
        *out_shape = (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = display_length,
            .decimals = 0U,
            .extra_flags = (!is_negative && magnitude > (uint64_t)INT64_MAX)
                               ? (uint32_t)MYLITE_RESULT_COLUMN_FLAG_UNSIGNED
                               : 0U,
            .nullable = false,
        };
        return MYLITE_OK;
    }

    return scalar_newdecimal_integer_literal_result_column_shape(&literal->span, out_shape);
}

static int scalar_newdecimal_integer_literal_result_column_shape(
    const struct mylite_sql_source_span *span,
    struct scalar_binary_numeric_result_column_shape *out_shape
) {
    size_t significant_digit_count = 0U;

    if (out_shape == NULL) {
        return MYLITE_MISUSE;
    }
    if (scalar_decimal_integer_significant_digit_count(span, &significant_digit_count) !=
            MYLITE_OK ||
        significant_digit_count == 0U ||
        significant_digit_count > literal_projection_max_significant_digits) {
        return MYLITE_ERROR;
    }
    *out_shape = (struct scalar_binary_numeric_result_column_shape){
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL,
        .display_length = (uint64_t)significant_digit_count + 1U,
        .decimals = 0U,
        .extra_flags = 0U,
        .nullable = false,
    };
    return MYLITE_OK;
}

static int scalar_decimal_integer_significant_digit_count(
    const struct mylite_sql_source_span *span,
    size_t *out_significant_digit_count
) {
    size_t first_significant_digit = 0U;

    if (span == NULL || span->text == NULL || out_significant_digit_count == NULL) {
        return MYLITE_MISUSE;
    }
    first_significant_digit = span->length;
    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (!isdigit(byte)) {
            return MYLITE_ERROR;
        }
        if (byte != (unsigned char)'0' && first_significant_digit == span->length) {
            first_significant_digit = index;
        }
    }
    *out_significant_digit_count =
        first_significant_digit == span->length ? 0U : span->length - first_significant_digit;
    return MYLITE_OK;
}

static uint64_t scalar_connection_max_bytes_per_character(const struct mylite_db *database) {
    const char *character_set_name =
        database == NULL ? NULL : database->session.character_set_connection;
    const struct mylite_execution_catalog_character_set *character_set =
        character_set_by_name(character_set_name);
    struct mylite_sql_source_span maxlen_span = {0};
    uint64_t maxlen = 0U;

    if (character_set == NULL) {
        return 4U;
    }
    maxlen_span = (struct mylite_sql_source_span){
        .text = character_set->maxlen,
        .length = strlen(character_set->maxlen),
    };
    if (parse_unsigned_integer_literal(&maxlen_span, &maxlen) != MYLITE_OK || maxlen == 0U) {
        return 4U;
    }
    return maxlen;
}

static void populate_scalar_binary_numeric_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_binary_numeric_result_column_shape shape
) {
    descriptor->logical_type = shape.logical_type;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = shape.display_length;
    descriptor->decimals = shape.decimals;
    descriptor->flags =
        MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM | shape.extra_flags;
    descriptor->nullable = shape.nullable;
    if (!shape.nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
}

static int populate_scalar_charset_collation_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const char *charset = NULL;
    const char *collation = NULL;
    struct session_scalar_cell cell = {0};
    uint64_t max_bytes_per_character = 4U;
    uint64_t display_length = 0U;
    size_t value_length = 0U;
    int rc = MYLITE_OK;

    if (descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_execution_scalar_expression_charset_collation_metadata(
        database,
        expression,
        &charset,
        &collation
    );
    if (rc == MYLITE_OK) {
        rc = session_scalar_value(database, expression, &cell);
    }
    if (rc != MYLITE_OK) {
        session_scalar_cell_deinit(&cell);
        return rc;
    }

    if (text_equals_ascii_case_insensitive(charset, "latin1") ||
        text_equals_ascii_case_insensitive(charset, "binary")) {
        max_bytes_per_character = 1U;
    } else if (text_equals_ascii_case_insensitive(charset, "utf8mb3")) {
        max_bytes_per_character = 3U;
    }
    value_length = cell.value == NULL ? 0U : strlen(cell.value);
    if (value_length > UINT64_MAX / max_bytes_per_character) {
        session_scalar_cell_deinit(&cell);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    display_length = (uint64_t)value_length * max_bytes_per_character;

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = result_metadata_collation_id(collation);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = 0U;
    descriptor->nullable = true;

    session_scalar_cell_deinit(&cell);
    return MYLITE_OK;
}

static void populate_scalar_connection_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_connection_string_result_column_shape shape
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = shape.display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = shape.extra_flags;
    descriptor->nullable = shape.nullable;
    if (!shape.nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
}

static void populate_uuid_string_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    bool nullable
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = mysql_collation_utf8mb3_general_ci_id;
    descriptor->collation_id = mysql_collation_utf8mb3_general_ci_id;
    descriptor->display_length = MYLITE_UUID_TEXT_SIZE;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = 0U;
    descriptor->nullable = nullable;
    if (!nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
}

static void populate_scalar_json_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_JSON;
    descriptor->charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = mysql_json_document_display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
}
