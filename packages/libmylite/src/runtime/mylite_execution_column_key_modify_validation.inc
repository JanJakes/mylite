static int validate_planned_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || !planned_column_is_char_or_varchar(column)) {
        set_runtime_error(database, "invalid string key column");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "string keys support only baseline CHAR descriptors",
            &length
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "string keys support only baseline VARCHAR descriptors",
            &length
        );
    }
    if (rc == MYLITE_OK && length == 0U) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && planned_column_is_varchar(column) &&
        length > varchar_string_key_max_length) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }

    return rc;
}

static int validate_descriptor_string_key_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || !column_descriptor_is_char_or_varchar(column)) {
        set_runtime_error(database, "invalid string key column descriptor");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &length);
    } else {
        rc = varchar_length_for_column(database, column, &length);
    }
    if (rc == MYLITE_OK && length == 0U) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && column_descriptor_is_varchar(column) &&
        length > varchar_string_key_max_length) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }

    return rc;
}

static int validate_string_key_value(
    struct mylite_db *database,
    const struct planned_value *value
) {
    if (value == NULL) {
        set_runtime_error(database, "invalid string key value");
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        return MYLITE_OK;
    }
    if (!value->is_text || !text_value_is_valid_string_key(value->text, value->text_length)) {
        set_unsupported_error(database, "string key values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool loaded_index_part_requires_string_key_validation(const struct loaded_index_part *part) {
    if (part == NULL) {
        return false;
    }
    if (column_descriptor_is_char_or_varchar(&part->column)) {
        return true;
    }

    if (!part->index_column.has_prefix_length) {
        return false;
    }

    return column_descriptor_is_text_family(&part->column);
}

static bool text_value_is_valid_string_key(const char *text, size_t text_length) {
    size_t character_count = 0U;

    if (text == NULL) {
        return false;
    }
    return validate_utf8_text(text, text_length, &character_count) == MYLITE_OK;
}

static bool text_value_is_supported_string_key(const char *text, size_t text_length) {
    if (text == NULL && text_length != 0U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\0' || byte > ascii_max_byte) {
            return false;
        }
    }

    return true;
}

static bool modify_column_integer_value_domain_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    const char *original_logical_type = original_column->logical_type;
    const char *replacement_logical_type = replacement_column->logical_type;

    if (original_logical_type == NULL || replacement_logical_type == NULL) {
        return false;
    }
    if (strcmp(original_logical_type, "TINYINT(1)") == 0) {
        original_logical_type = "TINYINT";
    }
    if (strcmp(replacement_logical_type, "TINYINT(1)") == 0) {
        replacement_logical_type = "TINYINT";
    }

    return strcmp(original_logical_type, replacement_logical_type) == 0;
}

static bool modify_column_char_varchar_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return ((column_descriptor_is_string_family(original_column) ||
             column_descriptor_is_binary_string_family(original_column)) &&
            planned_column_is_char_or_varchar(replacement_column)) != 0;
}

static bool modify_column_text_family_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return ((column_descriptor_is_string_family(original_column) ||
             column_descriptor_is_binary_string_family(original_column)) &&
            planned_column_is_text_family(replacement_column)) != 0;
}

static bool modify_column_binary_string_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    bool original_is_string = (column_descriptor_is_string_family(original_column) ||
                               column_descriptor_is_binary_string_family(original_column)) != 0;

    return (original_is_string && planned_column_is_binary_string_family(replacement_column)) != 0;
}

static bool modify_column_temporal_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    bool original_is_temporal = (column_descriptor_is_datetime(original_column) ||
                                 column_descriptor_is_timestamp(original_column)) != 0;
    bool replacement_is_temporal = (planned_column_is_datetime(replacement_column) ||
                                    planned_column_is_timestamp(replacement_column)) != 0;

    return (original_is_temporal && replacement_is_temporal) != 0;
}

static bool modify_column_numeric_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return (column_descriptor_is_numeric_replacement_family(original_column) &&
            planned_column_is_numeric_replacement_family(replacement_column)) != 0;
}

static bool modify_column_string_to_integer_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return (column_descriptor_is_string_family(original_column) && replacement_column != NULL &&
            replacement_column->physical_type != NULL &&
            strcmp(replacement_column->physical_type, "INTEGER") == 0) != 0;
}

static bool modify_column_integer_to_string_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    return (original_column != NULL && strcmp(original_column->physical_type, "INTEGER") == 0 &&
            planned_column_is_string_family(replacement_column)) != 0;
}

static bool column_descriptor_is_numeric_replacement_family(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return false;
    }
    return (strcmp(column->physical_type, "INTEGER") == 0 || column_descriptor_is_decimal(column) ||
            column_descriptor_is_approximate(column)) != 0;
}

static bool planned_column_is_numeric_replacement_family(const struct planned_column *column) {
    if (column == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->physical_type, "INTEGER") == 0 || planned_column_is_decimal(column) ||
            planned_column_is_approximate(column)) != 0;
}

static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node) {
    return mylite_sql_ast_node_nullability(nullability_node) != MYLITE_SQL_AST_NULLABILITY_NOT_NULL;
}
