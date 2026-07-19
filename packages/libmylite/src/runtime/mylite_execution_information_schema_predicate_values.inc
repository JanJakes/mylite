
static int information_schema_predicate_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    *out_value = (struct information_schema_predicate_value){
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE supports string, integer, DATABASE(), and SCHEMA() values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        value_node->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION) {
        return information_schema_predicate_function_value(database, value_node, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        size_t text_length = 0U;

        return decode_sql_string_literal(
            database,
            value_node,
            "INFORMATION_SCHEMA WHERE supports only string literals",
            "INFORMATION_SCHEMA WHERE string literals do not support NUL bytes",
            &out_value->text,
            &text_length
        );
    }

    return information_schema_predicate_integer_value_text(database, value_node, out_value);
}

static int information_schema_predicate_literal_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    if (value_node != NULL && (value_node->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
                               value_node->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION)) {
        *out_value = (struct information_schema_predicate_value){0};
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE IN and BETWEEN support only literal values"
        );
        return MYLITE_ERROR;
    }
    return information_schema_predicate_value_text(database, value_node, out_value);
}

static int information_schema_predicate_like_pattern_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct information_schema_predicate_value){
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE LIKE supports only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "INFORMATION_SCHEMA WHERE LIKE supports only string pattern literals",
        "INFORMATION_SCHEMA WHERE LIKE pattern literals do not support NUL bytes",
        &out_value->text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(out_value->text, text_length)) {
        free(out_value->text);
        *out_value = (struct information_schema_predicate_value){0};
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE LIKE pattern literals support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_predicate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = session_scalar_value(database, value_node, &cell);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (cell.value == NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    return duplicate_text(database, cell.value, &out_value->text);
}

static int information_schema_predicate_integer_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    char buffer[integer_text_capacity];
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports only decimal integer predicate values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports string, integer, DATABASE(), and SCHEMA() values"
            );
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
    }

    rc = information_schema_format_signed_magnitude(
        database,
        is_negative,
        magnitude,
        buffer,
        sizeof(buffer)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_numeric = true;
    return duplicate_text(database, buffer, &out_value->text);
}

static int information_schema_format_signed_magnitude(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
) {
    const uint64_t int64_positive_max = (uint64_t)INT64_MAX;
    const uint64_t int64_negative_abs_max = ((uint64_t)INT64_MAX) + 1U;
    int written = 0;

    if (is_negative) {
        if (magnitude > int64_negative_abs_max) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        if (magnitude == int64_negative_abs_max) {
            written = snprintf(buffer, buffer_size, "%" PRId64, INT64_MIN);
        } else {
            written = snprintf(buffer, buffer_size, "-%" PRIu64, magnitude);
        }
    } else {
        if (magnitude > int64_positive_max) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        written = snprintf(buffer, buffer_size, "%" PRIu64, magnitude);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA predicate value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
