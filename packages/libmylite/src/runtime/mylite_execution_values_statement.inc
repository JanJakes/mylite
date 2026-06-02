
static int execute_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *rows = child_at(statement, 0U);
    const struct mylite_sql_ast_node *order_clause =
        child_with_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        child_with_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    struct planned_select_limit limit = {
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    mylite_result *result = NULL;
    size_t column_count = 0U;
    int rc = validate_values_statement_rows(database, rows, &column_count);

    if (rc == MYLITE_OK) {
        rc = validate_values_order_clause(database, order_clause, column_count);
    }
    if (rc == MYLITE_OK) {
        rc = plan_values_limit(database, limit_clause, &limit);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_values_result_columns(database, result, column_count);
    if (rc == MYLITE_OK) {
        rc = append_values_result_rows(database, result, rows, &limit, column_count);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int validate_values_statement_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rows,
    size_t *out_column_count
) {
    const struct mylite_sql_ast_node *row = NULL;
    size_t expected_column_count = 0U;
    size_t row_number = 1U;
    int rc = MYLITE_OK;

    if (out_column_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column_count = 0U;
    if (rows == NULL || rows->kind != MYLITE_SQL_AST_VALUES_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    row = child_at(rows, 0U);
    if (row == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    expected_column_count = mylite_sql_ast_node_child_count(row);
    if (expected_column_count == 0U) {
        set_values_empty_row_error(database);
        return MYLITE_ERROR;
    }

    while (row != NULL) {
        rc = validate_values_statement_row(
            database,
            row,
            (struct values_statement_row_shape){
                .column_count = expected_column_count,
                .row_number = row_number,
            }
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        row = row->next_sibling;
        ++row_number;
    }

    *out_column_count = expected_column_count;
    return MYLITE_OK;
}

static int validate_values_statement_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
) {
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_count = 0U;

    if (row == NULL || row->kind != MYLITE_SQL_AST_VALUES_ROW) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    column_count = mylite_sql_ast_node_child_count(row);
    if (column_count == 0U) {
        set_values_empty_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_count != shape.column_count) {
        set_column_count_mismatch_error(database, shape.row_number);
        return MYLITE_ERROR;
    }

    value = child_at(row, 0U);
    while (value != NULL) {
        int rc = validate_values_statement_value(database, value);

        if (rc != MYLITE_OK) {
            return rc;
        }
        value = value->next_sibling;
    }
    return MYLITE_OK;
}

static int validate_values_statement_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        set_values_default_error(database);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return validate_values_integer_literal(database, value);
    }
    if (value->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return validate_values_integer_literal(database, value);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
        literal_kind == MYLITE_SQL_AST_LITERAL_NULL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
    );
    return MYLITE_ERROR;
}

static int validate_values_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;
    const uint64_t int64_negative_max_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = value;
    uint64_t magnitude = 0U;
    bool is_negative = false;

    if (value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "VALUES supports only signed integer literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "VALUES supports only signed integer literals");
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_values_integer_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    if ((!is_negative && magnitude > int64_positive_max) ||
        (is_negative && magnitude > int64_negative_max_magnitude)) {
        set_values_integer_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_values_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t column_count
) {
    const struct mylite_sql_ast_node *first_child = NULL;

    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_child = child_at(order_clause, 0U);
    if (first_child == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (first_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        const struct mylite_sql_ast_node *item = child_at(first_child, 0U);

        while (item != NULL) {
            int rc = validate_values_order_item(database, item, column_count);

            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
        }
        return MYLITE_OK;
    }

    return validate_values_order_key(database, first_child, column_count);
}

static int validate_values_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    size_t column_count
) {
    if (item == NULL || item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return validate_values_order_key(database, child_at(item, 0U), column_count);
}

static int validate_values_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
) {
    if (order_key == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (order_key->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(order_key) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return validate_values_order_ordinal(database, order_key, column_count);
    }
    if (order_key->kind == MYLITE_SQL_AST_IDENTIFIER ||
        order_key->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return validate_values_order_identifier(database, order_key, column_count);
    }

    set_unsupported_error(database, "VALUES ORDER BY supports only column names and ordinals");
    return MYLITE_ERROR;
}

static int validate_values_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
) {
    uint64_t ordinal = 0U;
    char *display_text = NULL;

    if (parse_unsigned_integer_literal(&order_key->span, &ordinal) == MYLITE_OK && ordinal >= 1U &&
        ordinal <= column_count) {
        return MYLITE_OK;
    }

    if (copy_source_span_text(database, &order_key->span, &display_text) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    set_unknown_order_column_error(database, display_text);
    free(display_text);
    return MYLITE_ERROR;
}

static int validate_values_order_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char display_text[MYLITE_CATALOG_IDENTIFIER_CAPACITY * table_name_part_capacity];
    size_t part_count = 0U;
    size_t column_index = 0U;
    int rc = collect_column_reference_parts(database, order_key, parts, &part_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U && values_column_name_to_index(parts[0], column_count, &column_index)) {
        return MYLITE_OK;
    }

    rc = format_column_reference_name(
        database,
        parts,
        part_count,
        display_text,
        sizeof(display_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_order_column_error(database, display_text);
    return MYLITE_ERROR;
}

static bool values_column_name_to_index(
    const char *column_name,
    size_t column_count,
    size_t *out_index
) {
    enum { decimal_radix = 10U };

    const char prefix[] = "column_";
    const char *digits = NULL;
    uint64_t value = 0U;

    if (out_index != NULL) {
        *out_index = 0U;
    }
    if (column_name == NULL || !text_has_ascii_case_insensitive_prefix(column_name, prefix)) {
        return false;
    }

    digits = column_name + sizeof(prefix) - 1U;
    if (*digits == '\0' || (digits[0] == '0' && digits[1] != '\0')) {
        return false;
    }
    for (const char *cursor = digits; *cursor != '\0'; ++cursor) {
        unsigned char byte = (unsigned char)*cursor;

        if (byte < '0' || byte > '9') {
            return false;
        }
        if (value > (UINT64_MAX - (uint64_t)(byte - '0')) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + (uint64_t)(byte - '0');
    }
    if (value >= column_count) {
        return false;
    }

    if (out_index != NULL) {
        *out_index = (size_t)value;
    }
    return true;
}

static int plan_values_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    if (out_limit == NULL) {
        return MYLITE_MISUSE;
    }
    *out_limit = (struct planned_select_limit){
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;
    if (child_at(limit_clause, 1U) != NULL) {
        rc =
            convert_limit_integer_literal(database, child_at(limit_clause, 1U), &out_limit->offset);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int append_values_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        char label[sizeof("column_") + uint64_decimal_digit_capacity];
        int written = snprintf(label, sizeof(label), "column_%zu", column_index);
        int rc = MYLITE_OK;

        if (written < 0 || (size_t)written >= sizeof(label)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        rc = mylite_result_append_column(result, label);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_values_result_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *rows,
    const struct planned_select_limit *limit,
    size_t column_count
) {
    const struct mylite_sql_ast_node *row = child_at(rows, 0U);
    uint64_t offset = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t row_index = 0U;
    uint64_t emitted = 0U;

    if (limit != NULL && limit->has_offset) {
        offset = (uint64_t)limit->offset;
    }
    if (limit != NULL && limit->has_limit) {
        row_count = (uint64_t)limit->row_count;
    }

    while (row != NULL) {
        int rc = MYLITE_OK;

        if (row_index >= offset && emitted < row_count) {
            rc = append_values_result_row(
                database,
                result,
                row,
                (struct values_statement_row_shape){
                    .column_count = column_count,
                    .row_number = (size_t)(row_index + 1U),
                }
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            ++emitted;
        }
        if (emitted >= row_count) {
            return MYLITE_OK;
        }
        row = row->next_sibling;
        ++row_index;
    }
    return MYLITE_OK;
}

static int append_values_result_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
) {
    const struct mylite_sql_ast_node *value = child_at(row, 0U);
    struct session_scalar_cell *cells = NULL;
    struct mylite_result_cell *result_cells = NULL;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (shape.column_count > SIZE_MAX / sizeof(*cells) ||
        shape.column_count > SIZE_MAX / sizeof(*result_cells)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(shape.column_count, sizeof(*cells));
    if (cells == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    result_cells = (struct mylite_result_cell *)calloc(shape.column_count, sizeof(*result_cells));
    if (result_cells == NULL) {
        session_scalar_cell_array_deinit(cells, shape.column_count);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (value != NULL && column_index < shape.column_count && rc == MYLITE_OK) {
        rc = values_statement_value_cell(database, value, &cells[column_index]);
        if (rc == MYLITE_OK) {
            result_cells[column_index] = session_scalar_cell_result_cell(&cells[column_index]);
        }
        value = value->next_sibling;
        ++column_index;
    }
    if (rc == MYLITE_OK && (value != NULL || column_index != shape.column_count)) {
        set_column_count_mismatch_error(database, shape.row_number);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, result_cells);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    session_scalar_cell_array_deinit(cells, shape.column_count);
    free(result_cells);
    return rc;
}

static int values_statement_value_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (value == NULL || out_cell == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        set_values_default_error(database);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return values_statement_integer_cell(database, value, out_cell);
    }
    if (value->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_cell->value = "1";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        int rc = decode_sql_string_literal_with_policy(
            database,
            value,
            "VALUES supports only ordinary string literals",
            "VALUES does not support NUL bytes in string literals",
            true,
            &out_cell->owned_text,
            &out_cell->value_size
        );

        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->owned_text;
            out_cell->has_value_size = true;
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return values_statement_integer_cell(database, value, out_cell);
    }

    set_unsupported_error(
        database,
        "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
    );
    return MYLITE_ERROR;
}

static int values_statement_integer_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = value;
    bool is_negative = false;
    int rc = validate_values_integer_literal(database, value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value);

        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        literal = child_at(value, 0U);
    }

    rc = normalize_decimal_integer_literal(
        database,
        &literal->span,
        is_negative,
        out_cell->literal_text,
        sizeof(out_cell->literal_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->literal_text;
    }
    return rc;
}
