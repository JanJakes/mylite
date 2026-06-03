static int materialize_dml_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool adjust_missing_default,
    struct planned_value *out_value
) {
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return materialize_integer_expression_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = column->default_integer,
        };
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL) {
        return copy_decimal_text_value(database, column->default_text, out_value);
    }
    if (column_descriptor_is_bit(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return convert_bit_literal(
            database,
            &(struct mylite_sql_ast_node){
                .kind = MYLITE_SQL_AST_LITERAL,
                .span = {.text = column->default_text, .length = strlen(column->default_text)},
                .payload.literal = {.kind = MYLITE_SQL_AST_LITERAL_BIT},
            },
            column,
            1U,
            false,
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return materialize_binary_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return materialize_character_expression_default_value(
            database,
            column,
            dml_allows_string_truncation_adjustment(database, adjust_missing_default),
            out_value
        );
    }
    if (column_descriptor_is_approximate(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return copy_approximate_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return copy_text_value(database, column->default_text, out_value);
    }
    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return make_current_timestamp_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return make_current_date_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return make_current_time_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        return convert_null_insert_value(database, column, adjust_missing_default, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE && column->is_nullable) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (!adjust_missing_default) {
        set_no_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return materialize_dml_missing_default_value(database, column, out_value);
}

static int materialize_integer_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_bit(column)) {
        return materialize_bit_expression_default_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_year_value(database, (uint32_t)column->default_integer, out_value);
    }

    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .integer = column->default_integer,
    };
    return MYLITE_OK;
}

static int materialize_bit_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    uint64_t magnitude = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (rc != MYLITE_OK || column->default_integer < 0) {
        return MYLITE_MISUSE;
    }
    magnitude = (uint64_t)column->default_integer;
    if (magnitude > bit_width_max_value(width)) {
        return MYLITE_MISUSE;
    }

    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = magnitude},
        out_value
    );
}

static int materialize_character_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
) {
    size_t text_length = column == NULL ? 0U : strlen(column->default_text);
    char *text = NULL;
    struct char_text_conversion conversion = {0};
    int rc = MYLITE_OK;

    if (column == NULL) {
        return MYLITE_MISUSE;
    }
    if (column_descriptor_is_text_family(column)) {
        return copy_text_value(database, column->default_text, out_value);
    }
    text = malloc(text_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, column->default_text, text_length + 1U);

    conversion = (struct char_text_conversion){
        .text = text,
        .text_length = text_length,
        .row_number = 1U,
    };
    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
    } else if (column_descriptor_is_varchar(column)) {
        rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
    } else {
        set_runtime_error(database, "invalid generated character default descriptor");
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    return assign_text_value(database, text, conversion.text_length, out_value);
}

static int materialize_binary_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (!column_descriptor_is_binary_string_family(column)) {
        set_runtime_error(database, "invalid binary default descriptor");
        return MYLITE_ERROR;
    }

    rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int validate_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct mylite_catalog_column_descriptor source_column = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = resolve_default_function_source_column(
        database,
        value_node,
        source_context,
        table_columns,
        table_column_count,
        &source_column
    );

    if (rc == MYLITE_OK) {
        rc = materialize_default_function_source_value(database, &source_column, &value);
    }
    planned_value_deinit(&value);
    return rc;
}

static int convert_default_function_value_for_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *target_column,
    bool ignore_errors,
    struct planned_value *out_value
) {
    struct mylite_catalog_column_descriptor source_column = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = resolve_default_function_source_column(
        database,
        value_node,
        source_context,
        table_columns,
        table_column_count,
        &source_column
    );

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (rc == MYLITE_OK &&
        !default_function_source_target_types_are_compatible(&source_column, target_column)) {
        set_unsupported_error(
            database,
            default_function_implicit_conversion_message(target_column)
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = materialize_default_function_source_value(database, &source_column, &value);
    }
    if (rc == MYLITE_OK && value.is_null && !target_column->is_nullable) {
        if (ignore_errors) {
            planned_value_deinit(&value);
            return convert_null_insert_value(database, target_column, true, out_value);
        }
        set_bad_null_error(database, target_column->name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_value = value;
        value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .is_blob = false,
            .is_real = false,
            .integer = 0,
            .real = 0.0,
            .text = NULL,
            .text_length = 0U,
        };
    }
    planned_value_deinit(&value);
    return rc;
}

static int resolve_default_function_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = NULL;
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION ||
        mylite_sql_ast_node_child_count(value_node) != 1U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    column_node = child_at(value_node, 0U);
    if (source_context == NULL || source_context->source == NULL) {
        rc = collect_column_reference_parts(database, column_node, parts, &part_count);
        if (rc == MYLITE_OK) {
            rc = format_column_reference_name(
                database,
                parts,
                part_count,
                column_name,
                sizeof(column_name)
            );
        }
        if (rc == MYLITE_OK) {
            set_unknown_column_reference_error(database, COLUMN_REFERENCE_FIELD, column_name);
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "DEFAULT() supports only descriptor column references",
        table_columns,
        table_column_count,
        out_column
    );
}

static int materialize_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_column,
    struct planned_value *out_value
) {
    if (source_column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};

    if (column_descriptor_is_auto_increment(source_column)) {
        *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (column_default_kind_is_expression(source_column->default_kind)) {
        set_default_function_expression_error(database);
        return MYLITE_ERROR;
    }
    if (source_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP ||
        source_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE ||
        source_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        set_unsupported_error(
            database,
            "DEFAULT() does not yet support current date/time column defaults"
        );
        return MYLITE_ERROR;
    }

    return materialize_dml_default_value(database, source_column, false, out_value);
}

static bool default_function_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (source_column == NULL || target_column == NULL) {
        return false;
    }
    if (source_column->column_id == target_column->column_id) {
        return true;
    }
    return (strcmp(source_column->logical_type, target_column->logical_type) == 0 &&
            strcmp(source_column->physical_type, target_column->physical_type) == 0) != 0;
}

static const char *default_function_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
) {
    (void)target_column;
    return "DEFAULT() assignment does not support implicit descriptor conversion";
}

static int materialize_dml_missing_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    rc = append_no_default_warning(database, column->name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column->is_nullable) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    } else if (column_descriptor_is_string_family(column) || column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    } else if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    } else if (column_descriptor_is_enum(column)) {
        return make_enum_first_label_value(database, column, out_value);
    } else if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    } else if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    } else if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    } else if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    } else if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};

        rc = decimal_type_info_for_logical_type(column->logical_type, &info);
        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    } else if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    } else if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    } else if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    } else if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    } else if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    } else {
        *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    }
    return MYLITE_OK;
}
