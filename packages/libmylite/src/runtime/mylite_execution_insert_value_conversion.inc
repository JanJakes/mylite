static bool dml_allows_missing_default_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
) {
    return (ignore_errors || !session_sql_mode_is_strict(database)) != 0;
}

static bool dml_allows_string_truncation_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
) {
    return dml_allows_missing_default_adjustment(database, ignore_errors);
}

static bool dml_value_is_rand_function(const struct mylite_sql_ast_node *value_node) {
    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }
    return value_node->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
           value_node->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION ||
           value_node->kind == MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR;
}

static int convert_dml_rand_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    bool *out_handled,
    struct planned_value *out_value
) {
    struct session_scalar_cell cell = {0};
    struct char_text_conversion conversion = {0};
    char *text = NULL;
    int rc = MYLITE_OK;

    if (out_handled == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    value_node = unwrap_parenthesized_expression(value_node);
    if (!dml_value_is_rand_function(value_node)) {
        return MYLITE_OK;
    }
    *out_handled = true;
    if (!column_descriptor_is_char(column) && !column_descriptor_is_varchar(column) &&
        !column_descriptor_is_text_family(column)) {
        set_unsupported_error(database, "DML RAND() values support only nonbinary string targets");
        return MYLITE_ERROR;
    }

    rc = rand_function_value(database, value_node, &cell);
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_cell_warnings(database, &cell);
    }
    if (rc == MYLITE_OK && cell.value == NULL) {
        set_runtime_error(database, "invalid RAND() DML value");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = duplicate_text(database, cell.value, &text);
    }
    if (rc == MYLITE_OK) {
        conversion = (struct char_text_conversion){
            .text = text,
            .text_length = strlen(text),
            .row_number = row_number,
        };
        if (column_descriptor_is_char(column)) {
            rc = convert_char_text(
                database,
                column,
                &conversion,
                allow_string_truncation_adjustment
            );
        } else if (column_descriptor_is_varchar(column)) {
            rc = convert_varchar_text(
                database,
                column,
                &conversion,
                allow_string_truncation_adjustment
            );
        } else {
            rc = convert_text_family_text(
                database,
                column,
                &conversion,
                allow_string_truncation_adjustment,
                warn_on_trailing_space_truncation
            );
        }
    }
    if (rc == MYLITE_OK) {
        out_value->is_null = false;
        out_value->is_text = true;
        out_value->is_blob = false;
        out_value->is_real = false;
        out_value->integer = 0;
        out_value->real = 0.0;
        out_value->text = text;
        out_value->text_length = conversion.text_length;
        text = NULL;
    }

    free(text);
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int convert_dml_constant_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    bool auto_increment_null_generates,
    bool allow_constant_arithmetic,
    bool *out_handled,
    struct planned_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    if (out_handled == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARAMETER && database != NULL &&
        database->active_bound_statement != NULL &&
        database->active_bound_statement->analyzing_unbound_parameters) {
        *out_handled = true;
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){
            .is_text = column_descriptor_is_char(column) || column_descriptor_is_varchar(column) ||
                       column_descriptor_is_text_family(column) ||
                       column_descriptor_is_json(column) || column_descriptor_is_date(column) ||
                       column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
                       column_descriptor_is_timestamp(column) ||
                       column_descriptor_is_decimal(column),
            .is_blob = column_descriptor_is_binary_string_family(column) ||
                       column_descriptor_is_spatial(column),
            .is_real = column_descriptor_is_approximate(column),
            .is_external_parameter = true,
            .external_parameter_index = mylite_sql_ast_node_parameter_index(value_node),
        };
        return MYLITE_OK;
    }
    if (!dml_value_is_constant_scalar_expression(value_node, allow_constant_arithmetic)) {
        return MYLITE_OK;
    }
    *out_handled = true;

    rc = session_scalar_value(database, value_node, &cell);
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_cell_warnings(database, &cell);
    }
    if (rc == MYLITE_OK && cell.value == NULL) {
        if (auto_increment_null_generates && column_descriptor_is_auto_increment(column)) {
            planned_value_deinit(out_value);
            *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else {
            rc = convert_null_insert_value(database, column, ignore_errors, out_value);
        }
    } else if (rc == MYLITE_OK) {
        rc = convert_dml_scalar_cell_for_column(
            database,
            &cell,
            column,
            diagnostic_column_name,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            warn_on_trailing_space_truncation,
            out_value
        );
    }
    if (rc == MYLITE_OK && value_node->kind == MYLITE_SQL_AST_PARAMETER) {
        out_value->is_external_parameter = true;
        out_value->external_parameter_index = mylite_sql_ast_node_parameter_index(value_node);
    }

    session_scalar_cell_deinit(&cell);
    return rc;
}

static bool dml_value_is_constant_scalar_expression(
    const struct mylite_sql_ast_node *value_node,
    bool allow_constant_arithmetic
) {
    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }
    if (dml_scalar_expression_contains_unsupported_source(value_node)) {
        return false;
    }
    if (allow_constant_arithmetic && dml_scalar_expression_is_constant_arithmetic(value_node)) {
        return true;
    }
    switch (value_node->kind) {
    case MYLITE_SQL_AST_PARAMETER:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_GENERIC_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_USER_VARIABLE:
        return true;
    default:
        return false;
    }
}

static bool dml_scalar_expression_is_constant_arithmetic(
    const struct mylite_sql_ast_node *value_node
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_MULTIPLY) {
        return false;
    }
    return is_scalar_arithmetic_projection_expression(value_node);
}

static bool dml_scalar_expression_contains_unsupported_source(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *child = NULL;
    size_t child_index = 0U;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }
    if (value_node->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY ||
        value_node->kind == MYLITE_SQL_AST_IDENTIFIER ||
        value_node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER ||
        value_node->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        return true;
    }

    child = value_node->first_child;
    while (child != NULL) {
        if (dml_scalar_expression_child_is_syntax_metadata(value_node, child_index, child)) {
            child = child->next_sibling;
            ++child_index;
            continue;
        }
        if (dml_scalar_expression_contains_unsupported_source(child)) {
            return true;
        }
        child = child->next_sibling;
        ++child_index;
    }
    return false;
}

static bool dml_scalar_expression_child_is_syntax_metadata(
    const struct mylite_sql_ast_node *value_node,
    size_t child_index,
    const struct mylite_sql_ast_node *child
) {
    if (value_node == NULL || child == NULL) {
        return false;
    }
    if (value_node->kind == MYLITE_SQL_AST_GENERIC_FUNCTION &&
        child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return true;
    }
    if (value_node->kind == MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION && child_index == 1U) {
        return true;
    }
    if (value_node->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION && child_index == 1U) {
        return true;
    }
    if (value_node->kind == MYLITE_SQL_AST_DATE_ADD_FUNCTION &&
        child == mylite_execution_date_interval_unit_node(value_node)) {
        return true;
    }
    return false;
}

static int convert_dml_scalar_cell_for_column(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
) {
    struct decimal_type_info decimal_info = {0};
    struct approximate_type_info approximate_info = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (cell == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (column_descriptor_is_char(column)) {
        return convert_dml_scalar_cell_text_value(
            database,
            cell,
            column,
            row_number,
            allow_string_truncation_adjustment,
            false,
            out_value
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return convert_dml_scalar_cell_text_value(
            database,
            cell,
            column,
            row_number,
            allow_string_truncation_adjustment,
            false,
            out_value
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return convert_dml_scalar_cell_text_value(
            database,
            cell,
            column,
            row_number,
            allow_string_truncation_adjustment,
            warn_on_trailing_space_truncation,
            out_value
        );
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return convert_dml_scalar_cell_binary_value(
            database,
            cell,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_spatial(column)) {
        return convert_dml_scalar_cell_spatial_value(database, cell, out_value);
    }

    rc = copy_dml_scalar_cell_text(
        database,
        cell,
        "DML constant scalar values do not support NUL bytes for this target",
        false,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (column_descriptor_is_json(column)) {
        return normalize_json_value(
            database,
            text,
            text_length,
            diagnostic_column_name == NULL ? column->name : diagnostic_column_name,
            out_value
        );
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_string_value(
            database,
            text,
            text_length,
            column,
            row_number,
            dml_allows_missing_default_adjustment(database, ignore_errors),
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_relaxed_date_storage_text(
            database,
            text,
            text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return canonicalize_time_text(
            database,
            text,
            text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_relaxed_datetime_storage_text(
            database,
            text,
            text_length,
            column->name,
            row_number,
            ignore_errors,
            database == NULL ? 0 : database->session.time_zone_offset_minutes,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_relaxed_timestamp_storage_text(
            database,
            text,
            text_length,
            column->name,
            row_number,
            ignore_errors,
            0,
            out_value
        );
    }
    if (column_descriptor_is_decimal(column)) {
        rc = decimal_type_info_for_logical_type(column->logical_type, &decimal_info);
        if (rc == MYLITE_OK) {
            rc = convert_decimal_string_text(
                database,
                text,
                text_length,
                column,
                row_number,
                ignore_errors,
                &decimal_info,
                out_value
            );
        }
        free(text);
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        rc = approximate_type_info_for_logical_type(column->logical_type, &approximate_info);
        if (rc == MYLITE_OK) {
            rc = convert_approximate_string_text(
                database,
                text,
                text_length,
                column,
                row_number,
                ignore_errors,
                &approximate_info,
                out_value
            );
        }
        free(text);
        return rc;
    }
    if (column_descriptor_is_bit(column)) {
        rc = convert_dml_scalar_cell_bit_value(
            database,
            text,
            text_length,
            column,
            row_number,
            ignore_errors,
            out_value
        );
        free(text);
        return rc;
    }
    rc = convert_integer_string_text(
        database,
        text,
        text_length,
        column,
        row_number,
        ignore_errors,
        out_value
    );
    free(text);
    return rc;
}

static int convert_dml_scalar_cell_spatial_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    struct planned_value *out_value
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = copy_dml_scalar_cell_text(
        database,
        cell,
        "Spatial DML supports only geometry values",
        true,
        &bytes,
        &byte_count
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!mylite_spatial_geometry_bytes_are_valid(bytes, byte_count)) {
        free(bytes);
        set_cannot_get_geometry_object_error(database);
        return MYLITE_ERROR;
    }
    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int convert_dml_scalar_cell_text_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
) {
    struct char_text_conversion conversion = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int rc = copy_dml_scalar_cell_text(
        database,
        cell,
        "DML constant scalar string values do not support invalid NUL bytes",
        false,
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    conversion = (struct char_text_conversion){
        .text = text,
        .text_length = text_length,
        .row_number = row_number,
    };
    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_varchar(column)) {
        rc =
            convert_varchar_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else {
        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_string_truncation_adjustment,
            warn_on_trailing_space_truncation
        );
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }
    return assign_text_value(database, text, conversion.text_length, out_value);
}

static int convert_dml_scalar_cell_binary_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = copy_dml_scalar_cell_text(
        database,
        cell,
        "invalid binary string scalar value",
        true,
        &bytes,
        &byte_count
    );

    if (rc == MYLITE_OK) {
        rc = convert_binary_bytes_for_column(
            database,
            column,
            row_number,
            ignore_errors,
            &bytes,
            &byte_count
        );
    }
    if (rc != MYLITE_OK) {
        free(bytes);
        return rc;
    }
    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int convert_dml_scalar_cell_bit_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint64_t magnitude = 0U;
    bool parse_overflow = false;
    int rc = bit_string_bytes_to_magnitude(text, text_length, &magnitude, &parse_overflow);

    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (parse_overflow) {
        if (!ignore_errors) {
            set_data_too_long_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return clip_bit_value_for_column(database, column, row_number, false, out_value);
    }
    return convert_bit_value_magnitude(
        database,
        column,
        magnitude,
        false,
        row_number,
        ignore_errors,
        out_value
    );
}

static int copy_dml_scalar_cell_text(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
) {
    const char *value = cell == NULL ? NULL : cell->value;
    size_t value_length = 0U;
    char *copy = NULL;

    if (value == NULL || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    value_length = cell->has_value_size ? cell->value_size : strlen(value);
    if (!allow_nul && memchr(value, '\0', value_length) != NULL) {
        set_unsupported_error(database, nul_message);
        return MYLITE_ERROR;
    }
    copy = malloc(value_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (value_length != 0U) {
        memcpy(copy, value, value_length);
    }
    copy[value_length] = '\0';

    *out_text = copy;
    *out_text_length = value_length;
    return MYLITE_OK;
}

static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    bool handled = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return convert_insert_unix_timestamp_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_auto_increment(column)) {
        return convert_auto_increment_insert_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        return materialize_dml_default_value(
            database,
            column,
            dml_allows_missing_default_adjustment(database, ignore_errors),
            out_value
        );
    }
    rc = convert_statement_time_value_for_column(database, value_node, column, out_value, &handled);
    if (handled) {
        return rc;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return convert_null_insert_value(database, column, ignore_errors, out_value);
    }
    rc = convert_dml_rand_value(
        database,
        value_node,
        column,
        row_number,
        allow_string_truncation_adjustment,
        false,
        &handled,
        out_value
    );
    if (handled) {
        return rc;
    }
    rc = convert_dml_constant_scalar_value(
        database,
        value_node,
        column,
        diagnostic_column_name,
        row_number,
        ignore_errors,
        allow_string_truncation_adjustment,
        false,
        true,
        true,
        &handled,
        out_value
    );
    if (handled) {
        return rc;
    }

    return convert_insert_value_by_descriptor(
        database,
        value_node,
        column,
        diagnostic_column_name,
        row_number,
        ignore_errors,
        allow_string_truncation_adjustment,
        out_value
    );
}

static bool insert_value_is_unix_timestamp_arithmetic(const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_unix_timestamp_now_node_is_admitted(value_node)) {
        return true;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(value_node) != 2U) {
        return false;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return false;
    }

    left = child_at(value_node, 0U);
    right = child_at(value_node, 1U);
    return (insert_unix_timestamp_now_node_is_admitted(left) &&
            insert_unix_timestamp_delta_node_is_admitted(right)) != 0;
}

static int convert_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    static const struct unix_timestamp_arithmetic_messages messages = {
        .unsupported =
            "INSERT UNIX_TIMESTAMP arithmetic supports only UNIX_TIMESTAMP(), "
            "UNIX_TIMESTAMP() +/- signed integer literals, and UNIX_TIMESTAMP() +/- NULL",
        .delta_range =
            "INSERT UNIX_TIMESTAMP arithmetic supports only signed 64-bit integer deltas",
    };
    struct integer_column_range range = {0};
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (column_descriptor_is_auto_increment(column)) {
        set_unsupported_error(
            database,
            "INSERT UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT targets"
        );
        return MYLITE_ERROR;
    }

    if (!column_descriptor_is_string_family(column)) {
        rc = integer_range_for_column(
            database,
            column,
            "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string targets",
            &range
        );
        (void)range;
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = evaluate_insert_unix_timestamp_arithmetic_value(database, value_node, &messages, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        return convert_null_insert_value(database, column, ignore_errors, out_value);
    }

    if (column_descriptor_is_string_family(column)) {
        return finish_insert_unix_timestamp_text_value(
            database,
            value.integer,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    return finish_insert_unix_timestamp_integer_value(
        database,
        value.integer,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int evaluate_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
) {
    const struct mylite_sql_ast_node *right = NULL;
    struct scalar_arithmetic_value delta = {.is_null = false, .integer = 0};
    int64_t result = 0;
    bool overflow = false;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (messages == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){
        .is_null = false,
        .integer = current_timestamp_epoch(database),
    };

    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_unix_timestamp_now_node_is_admitted(value_node)) {
        return MYLITE_OK;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(value_node) != 2U ||
        !insert_unix_timestamp_now_node_is_admitted(child_at(value_node, 0U))) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    right = child_at(value_node, 1U);
    rc = evaluate_insert_unix_timestamp_delta(database, right, messages, &delta);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (delta.is_null) {
        out_value->is_null = true;
        out_value->integer = 0;
        return MYLITE_OK;
    }

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_ADD) {
        overflow = checked_int64_add(out_value->integer, delta.integer, &result);
    } else {
        overflow = checked_int64_subtract(out_value->integer, delta.integer, &result);
    }
    if (overflow) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }

    out_value->integer = result;
    return MYLITE_OK;
}

static int evaluate_insert_unix_timestamp_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    if (messages == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            out_value->is_null = true;
            return MYLITE_OK;
        }
        return parse_insert_unix_timestamp_delta_literal(
            database,
            value_node,
            false,
            messages->delta_range,
            out_value
        );
    }
    if (value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operand = unwrap_parenthesized_expression(child_at(value_node, 0U));
    return parse_insert_unix_timestamp_delta_literal(
        database,
        operand,
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        messages->delta_range,
        out_value
    );
}

static int parse_insert_unix_timestamp_delta_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *delta_range_message,
    struct scalar_arithmetic_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    uint64_t limit = (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;

    if (is_negative) {
        limit = int64_min_magnitude;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        magnitude > limit) {
        set_unsupported_error(database, delta_range_message);
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == int64_min_magnitude) {
        out_value->integer = INT64_MIN;
    } else if (is_negative) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int finish_insert_unix_timestamp_integer_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    bool is_negative = integer < 0;
    uint64_t magnitude = 0U;

    if (integer == INT64_MIN) {
        magnitude = int64_min_magnitude;
    } else if (is_negative) {
        magnitude = (uint64_t)-integer;
    } else {
        magnitude = (uint64_t)integer;
    }

    out_value->is_null = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
}

static int finish_insert_unix_timestamp_text_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct char_text_conversion conversion = {
        .text = NULL,
        .text_length = 0U,
        .row_number = row_number,
    };
    int rc = format_insert_unix_timestamp_integer_text(database, integer, &text, &text_length);

    if (rc != MYLITE_OK) {
        return rc;
    }
    conversion.text = text;
    conversion.text_length = text_length;

    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_varchar(column)) {
        rc =
            convert_varchar_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_text_family(column)) {
        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_string_truncation_adjustment,
            false
        );
    } else {
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(text);
        if (rc == MYLITE_ERROR && !diagnostics_has_error_condition(&database->diagnostics)) {
            set_unsupported_error(
                database,
                "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string "
                "targets"
            );
        }
        return rc;
    }

    return assign_text_value(database, text, conversion.text_length, out_value);
}

static int format_insert_unix_timestamp_integer_text(
    struct mylite_db *database,
    int64_t integer,
    char **out_text,
    size_t *out_text_length
) {
    char buffer[integer_text_capacity];
    char *text = NULL;
    int written = 0;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    written = snprintf(buffer, sizeof(buffer), "%" PRId64, integer);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        set_runtime_error(database, "failed to format INSERT UNIX_TIMESTAMP arithmetic value");
        return MYLITE_ERROR;
    }

    text = malloc((size_t)written + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, buffer, (size_t)written + 1U);
    *out_text = text;
    *out_text_length = (size_t)written;
    return MYLITE_OK;
}

static bool insert_unix_timestamp_now_node_is_admitted(const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION &&
            mylite_sql_ast_node_child_count(value_node) == 0U) != 0;
}

static bool insert_unix_timestamp_delta_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        return (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return false;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return false;
    }

    operand = unwrap_parenthesized_expression(child_at(value_node, 0U));
    return (operand != NULL && operand->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(operand) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
}

static int convert_insert_value_for_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    struct select_source_context source_context = {0};
    char diagnostic_column_name[(MYLITE_CATALOG_IDENTIFIER_CAPACITY * 2U) + 2U];
    const char *json_diagnostic_column_name = NULL;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || plan == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            planned_value_deinit(out_value);
            *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
            return MYLITE_OK;
        }
        set_generated_column_value_error(database, column->name, plan->table.name);
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        if (column_descriptor_is_json(column)) {
            int written = snprintf(
                diagnostic_column_name,
                sizeof(diagnostic_column_name),
                "%s.%s",
                plan->table.name,
                column->name
            );

            if (written < 0 || (size_t)written >= sizeof(diagnostic_column_name)) {
                set_runtime_error(database, "failed to format JSON diagnostic column name");
                return MYLITE_ERROR;
            }
            json_diagnostic_column_name = diagnostic_column_name;
        }
        return convert_insert_value(
            database,
            value_node,
            column,
            json_diagnostic_column_name,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            out_value
        );
    }

    (void)row_number;
    (void)ignore_errors;
    (void)allow_string_truncation_adjustment;
    source_context.source = &plan->target;
    return convert_default_function_value_for_target(
        database,
        value_node,
        &source_context,
        plan->columns,
        plan->column_count,
        column,
        ignore_errors,
        out_value
    );
}

static int convert_insert_value_by_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    if (column_descriptor_is_char(column)) {
        return convert_char_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return convert_varchar_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return convert_text_family_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            false,
            out_value
        );
    }
    if (column_descriptor_is_json(column)) {
        (void)row_number;
        (void)ignore_errors;
        return convert_json_literal(
            database,
            value_node,
            column,
            diagnostic_column_name,
            out_value
        );
    }
    if (column_descriptor_is_enum(column)) {
        (void)ignore_errors;
        return convert_enum_literal(database, value_node, column, row_number, out_value);
    }
    if (column_descriptor_is_set(column)) {
        (void)ignore_errors;
        return convert_set_literal(database, value_node, column, row_number, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return convert_binary_string_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_decimal(column)) {
        return convert_decimal_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_approximate(column)) {
        return convert_approximate_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_date_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return convert_time_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_spatial(column)) {
        (void)row_number;
        (void)ignore_errors;
        set_cannot_get_geometry_object_error(database);
        return MYLITE_ERROR;
    }

    return convert_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_auto_increment_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE ||
        (value_node->kind == MYLITE_SQL_AST_LITERAL &&
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL)) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    {
        bool handled = false;
        int rc = convert_dml_constant_scalar_value(
            database,
            value_node,
            column,
            NULL,
            row_number,
            ignore_errors,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            false,
            true,
            true,
            &handled,
            out_value
        );

        if (handled || rc != MYLITE_OK) {
            return rc;
        }
    }

    return convert_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_null_insert_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (column->is_nullable) {
        return MYLITE_OK;
    }
    if (!ignore_errors) {
        if (column_descriptor_is_spatial(column)) {
            set_spatial_bad_null_error(database, column->name);
            return MYLITE_ERROR;
        }
        set_bad_null_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = append_bad_null_warning(database, column->name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    }
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};

        rc = decimal_type_info_for_logical_type(column->logical_type, &info);
        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    }
    if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->integer = 0;
    return MYLITE_OK;
}
