static bool select_statement_is_scalar_projection(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        return false;
    }
    if (child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_scalar_projection_select_item_expression(child_at(select_item, 0U))) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return true;
}

static bool is_scalar_projection_select_item_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING) {
        return true;
    }
    return is_scalar_projection_expression(expression);
}

static bool select_statement_is_row_function_scalar_projection(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool found_row_function = false;

    if (!select_statement_is_scalar_projection(statement)) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        if (is_string_metadata_projection_expression(expression) ||
            is_hex_projection_expression(expression) ||
            is_base64_projection_expression(expression) ||
            is_unhex_projection_expression(expression) ||
            is_uuid_projection_expression(expression) ||
            is_char_projection_expression(expression) ||
            is_json_introspection_projection_expression(expression) ||
            is_json_extract_projection_expression(expression) ||
            is_json_value_projection_expression(expression) ||
            is_json_contains_projection_expression(expression) ||
            is_json_unquote_projection_expression(expression) ||
            is_json_set_projection_expression(expression) ||
            is_json_construction_projection_expression(expression)) {
            found_row_function = true;
        } else if (row_scalar_expression_contains_row_function(expression)) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return found_row_function;
}

static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);

    if (from_clause == NULL) {
        return true;
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return true;
    }
    return false;
}

static int reject_bare_native_function_identifier_lookup_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (!select_statement_has_no_source_or_dual(statement) || child_at(statement, 2U) != NULL) {
        return MYLITE_OK;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) != 1U) {
        return MYLITE_OK;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return MYLITE_OK;
    }
    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(expression, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(column_name, "pi") &&
        !text_equals_ascii_case_insensitive(column_name, "rand") &&
        !text_equals_ascii_case_insensitive(column_name, "sqrt") &&
        !text_equals_ascii_case_insensitive(column_name, "degrees") &&
        !text_equals_ascii_case_insensitive(column_name, "radians") &&
        !text_equals_ascii_case_insensitive(column_name, "acos") &&
        !text_equals_ascii_case_insensitive(column_name, "asin") &&
        !text_equals_ascii_case_insensitive(column_name, "sin") &&
        !text_equals_ascii_case_insensitive(column_name, "cos") &&
        !text_equals_ascii_case_insensitive(column_name, "tan") &&
        !text_equals_ascii_case_insensitive(column_name, "cot") &&
        !text_equals_ascii_case_insensitive(column_name, "atan") &&
        !text_equals_ascii_case_insensitive(column_name, "atan2") &&
        !text_equals_ascii_case_insensitive(column_name, "exp") &&
        !text_equals_ascii_case_insensitive(column_name, "ln") &&
        !text_equals_ascii_case_insensitive(column_name, "log") &&
        !text_equals_ascii_case_insensitive(column_name, "log10") &&
        !text_equals_ascii_case_insensitive(column_name, "log2") &&
        !text_equals_ascii_case_insensitive(column_name, "pow") &&
        !text_equals_ascii_case_insensitive(column_name, "power") &&
        !text_equals_ascii_case_insensitive(column_name, "uuid")) {
        return MYLITE_OK;
    }

    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

static bool select_statement_is_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool saw_scalar_projection_expression = false;

    if (!select_statement_has_no_source_or_dual(statement)) {
        return false;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_scalar_projection_attempt_expression(child_at(select_item, 0U))) {
            return false;
        }
        saw_scalar_projection_expression = true;
        select_item = select_item->next_sibling;
    }

    return saw_scalar_projection_expression;
}

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

static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    struct session_scalar_cell *cells = NULL;
    struct mylite_result_cell *values = NULL;
    mylite_result *result = NULL;
    size_t column_count = mylite_sql_ast_node_child_count(select_list);
    size_t evaluated_column_count = 0U;
    uint64_t previous_last_insert_id = database->session.last_insert_id;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (column_count > SIZE_MAX / sizeof(*values) || column_count > SIZE_MAX / sizeof(*cells)) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(column_count, sizeof(*cells));
    if (cells == NULL) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct mylite_result_cell *)calloc(column_count, sizeof(*values));
    if (values == NULL) {
        session_scalar_cell_array_deinit(cells, 0U);
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = append_select_modifier_warnings(database, statement);
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_select_warnings(database, select_list);
    }
    if (rc == MYLITE_OK) {
        rc = append_scalar_projection_columns_and_values(
            database,
            select_item,
            result,
            cells,
            values,
            &evaluated_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK || evaluated_column_count != 0U) {
        int warning_rc =
            append_session_scalar_cells_warnings(database, cells, evaluated_column_count);

        if (warning_rc != MYLITE_OK) {
            rc = warning_rc;
        }
    }
    free(values);
    session_scalar_cell_array_deinit(cells, evaluated_column_count);
    if (rc != MYLITE_OK) {
        database->session.last_insert_id = previous_last_insert_id;
        mylite_result_free(result);
        return rc;
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_scalar_projection_columns_and_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    mylite_result *result,
    struct session_scalar_cell *cells,
    struct mylite_result_cell *values,
    size_t *out_column_count
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);
        char *column_name = NULL;

        if (alias != NULL) {
            rc = copy_select_item_alias_text(database, alias, &column_name);
        } else {
            rc = copy_scalar_projection_column_name(database, expression, &column_name);
        }
        if (rc == MYLITE_OK) {
            rc = append_scalar_projection_result_column(database, expression, column_name, result);
        }
        free(column_name);
        if (rc == MYLITE_OK) {
            rc = session_scalar_value(database, expression, &cells[column_index]);
        }
        if (rc == MYLITE_OK) {
            values[column_index] = session_scalar_cell_result_cell(&cells[column_index]);
            ++column_index;
            select_item = select_item->next_sibling;
        }
    }

    *out_column_count = column_index;
    return rc;
}

static int append_scalar_projection_result_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *column_name,
    mylite_result *result
) {
    struct mylite_result_column_descriptor descriptor;
    int rc = make_scalar_result_column_descriptor(database, expression, column_name, &descriptor);

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

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

static struct mylite_result_cell session_scalar_cell_result_cell(
    const struct session_scalar_cell *cell
) {
    size_t value_size = 0U;

    if (cell == NULL || cell->value == NULL) {
        return (struct mylite_result_cell){.is_null = true};
    }
    value_size = strlen(cell->value);
    if (cell->has_value_size) {
        value_size = cell->value_size;
    }

    return (struct mylite_result_cell){
        .bytes = cell->value,
        .size = value_size,
        .is_null = false,
    };
}

static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    unwrapped = unwrap_parenthesized_expression(expression);
    if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(unwrapped, 0U));

        while (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
               mylite_sql_ast_node_operator(literal) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            literal = unwrap_parenthesized_expression(child_at(literal, 0U));
        }
        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
            enum mylite_sql_ast_literal_kind literal_kind =
                mylite_sql_ast_node_literal_kind(literal);

            if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
                return copy_source_span_text(database, &literal->span, out_text);
            }
        }
    }
    if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(unwrapped);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return copy_source_span_text(database, &unwrapped->span, out_text);
        }
    }

    return copy_source_span_text(database, &expression->span, out_text);
}

static bool do_statement_has_only_scalar_projection_expressions(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = NULL;

    if (expression_list == NULL || expression_list->kind != MYLITE_SQL_AST_DO_EXPRESSION_LIST) {
        return false;
    }
    expression = child_at(expression_list, 0U);
    if (expression == NULL) {
        return false;
    }
    while (expression != NULL) {
        if (is_scalar_subquery_projection_expression(expression) ||
            !is_scalar_projection_expression(expression)) {
            return false;
        }
        expression = expression->next_sibling;
    }

    return true;
}

static int append_session_scalar_do_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && expression != NULL) {
        rc = append_session_scalar_expression_warnings(database, expression);
        expression = expression->next_sibling;
    }

    return rc;
}

static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression =
            unwrap_parenthesized_expression(child_at(select_item, 0U));

        rc = append_session_scalar_expression_warnings(database, expression);
        select_item = select_item->next_sibling;
    }

    return rc;
}

static int append_session_scalar_expression_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (current->kind == MYLITE_SQL_AST_FOUND_ROWS_FUNCTION) {
            rc = append_found_rows_deprecation_warning(database);
        } else if (current->kind == MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
            rc = append_convert_using_charset_warning(database, current);
        } else if (current->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
            enum mylite_execution_system_variable_kind variable =
                MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;

            rc = resolve_session_system_variable(database, current, &variable);
            if (rc == MYLITE_OK &&
                mylite_execution_system_variable_warns_on_scalar_read(variable)) {
                rc = append_system_variable_read_warning(database, variable);
            }
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; rc == MYLITE_OK && child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int append_select_modifier_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    unsigned int options = mylite_sql_ast_node_select_options(statement);

    if ((options & MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE) == 0U) {
        return MYLITE_OK;
    }

    return append_sql_no_cache_deprecation_warning(database);
}

static int append_found_rows_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_found_rows_deprecated,
        "HY000",
        "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using "
        "COUNT(*) instead."
    );
}

static int append_sql_calc_found_rows_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_sql_calc_found_rows_deprecated,
        "HY000",
        "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider "
        "using two separate queries instead."
    );
}

static int append_sql_no_cache_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_sql_no_cache_deprecated,
        "HY000",
        "'SQL_NO_CACHE' is deprecated and will be removed in a future release."
    );
}

static int append_convert_using_charset_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_convert_charset_info info = {0};
    int rc = scalar_convert_charset_info_for_expression(database, expression, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    switch (info.warning) {
    case SCALAR_CONVERT_CHARSET_WARNING_UTF8_ALIAS:
        return append_utf8_alias_warning(database);
    case SCALAR_CONVERT_CHARSET_WARNING_UTF8MB3_DEPRECATED:
        return append_utf8mb3_deprecation_warning(database);
    case SCALAR_CONVERT_CHARSET_WARNING_NONE:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int append_utf8_alias_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_utf8_alias,
        "HY000",
        "'utf8' is currently an alias for the character set UTF8MB3, but will be an alias "
        "for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be "
        "unambiguous."
    );
}

static int append_utf8mb3_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_utf8mb3_deprecated,
        "HY000",
        "'utf8mb3' is deprecated and will be removed in a future release. Please use "
        "utf8mb4 instead"
    );
}

static int accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
) {
    return accumulate_staged_warning_count(database, cell_warning_count, total_warning_count);
}

static int accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
) {
    if (total_warning_count == NULL) {
        return MYLITE_MISUSE;
    }
    if (*total_warning_count > SIZE_MAX - cell_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *total_warning_count += cell_warning_count;
    return MYLITE_OK;
}

static int append_session_scalar_cell_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cell
) {
    int rc = MYLITE_OK;

    if (cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (cell->has_staged_truncated_integer_warning) {
        rc = append_truncated_incorrect_integer_warning(
            database,
            cell->staged_truncated_integer_text
        );
    }
    if (rc == MYLITE_OK && cell->has_staged_truncated_decimal_warning) {
        rc = append_truncated_incorrect_decimal_warning(
            database,
            cell->staged_truncated_decimal_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_signed_complement_warnings(
            database,
            cell->staged_signed_complement_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_unsigned_complement_warnings(
            database,
            cell->staged_unsigned_complement_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_division_by_zero_warnings(database, cell->staged_division_by_zero_warning_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_invalid_logarithm_warnings(
            database,
            cell->staged_invalid_logarithm_warning_count
        );
    }
    if (rc == MYLITE_OK && cell->has_staged_unhex_incorrect_string_warning) {
        rc = append_unhex_incorrect_string_warning(
            database,
            cell->staged_unhex_incorrect_string_text
        );
    }
    return rc;
}

static int append_session_scalar_cells_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cells,
    size_t cell_count
) {
    int rc = MYLITE_OK;

    if (cells == NULL && cell_count != 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < cell_count; ++index) {
        rc = append_session_scalar_cell_warnings(database, &cells[index]);
    }
    return rc;
}

static int append_division_by_zero_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_division_by_zero,
            "22012",
            "Division by 0"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_invalid_logarithm_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_invalid_argument_for_logarithm,
            "HY000",
            "Invalid argument for logarithm"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const char *value_text
) {
    static const char prefix[] = "Truncated incorrect INTEGER value: '";
    static const char suffix[] = "'";
    char *message = NULL;
    size_t value_length = 0U;
    size_t message_length = 0U;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        return MYLITE_MISUSE;
    }
    value_length = strlen(value_text);
    if (value_length > SIZE_MAX - sizeof(prefix) - sizeof(suffix)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    message_length = (sizeof(prefix) - 1U) + value_length + (sizeof(suffix) - 1U);
    message = (char *)malloc(message_length + 1U);
    if (message == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(message, prefix, sizeof(prefix) - 1U);
    memcpy(message + sizeof(prefix) - 1U, value_text, value_length);
    memcpy(message + sizeof(prefix) - 1U + value_length, suffix, sizeof(suffix));

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_integer,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    free(message);
    return rc;
}

static int append_truncated_incorrect_decimal_warning(
    struct mylite_db *database,
    const char *value_text
) {
    char
        message[sizeof("Truncated incorrect DECIMAL value: ''") + literal_projection_text_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        return MYLITE_MISUSE;
    }
    written =
        snprintf(message, sizeof(message), "Truncated incorrect DECIMAL value: '%s'", value_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format CONV() warning");
        return MYLITE_ERROR;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_decimal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_signed_complement_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_cast_complement,
            "HY000",
            "Cast to signed converted positive out-of-range integer to its negative complement"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_unsigned_complement_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_cast_complement,
            "HY000",
            "Cast to unsigned converted negative integer to its positive complement"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_unhex_incorrect_string_warning(
    struct mylite_db *database,
    const char *value_text
) {
    return mylite_string_unhex_append_incorrect_warning(
        database,
        value_text,
        value_text == NULL ? 0U : strlen(value_text)
    );
}

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
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
        return "JSON_LENGTH";
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
        return "JSON_KEYS";
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
        return "JSON_TYPE";
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
        return "JSON_SET";
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
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
        return "HEX";
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
        return "TO_BASE64";
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
        return "FROM_BASE64";
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
        return "UNHEX";
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
        return "UUID";
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
