static int append_information_schema_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    struct primary_key_info primary_key = mylite_execution_primary_key_info_init();
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_primary_key_info(database, table->table_id, columns, column_count, &primary_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_columns_base_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index],
            &primary_key,
            indexes,
            index_count
        );
    }
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    mylite_execution_primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static int append_information_schema_st_geometry_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_st_geometry_columns_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index]
        );
    }
    free(columns);
    return rc;
}

static int append_information_schema_columns_extensions_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_columns_extensions_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index]
        );
    }
    free(columns);
    return rc;
}

static int append_information_schema_columns_view_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    struct primary_key_info primary_key = mylite_execution_primary_key_info_init();
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_columns_base_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index],
            &primary_key,
            NULL,
            0U
        );
    }
    mylite_execution_primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static int append_information_schema_columns_base_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct primary_key_info *primary_key,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    char ordinal_text[integer_text_capacity];
    char default_text[generated_default_display_text_capacity];
    char character_length_text[integer_text_capacity];
    char character_octet_text[integer_text_capacity];
    char numeric_precision_text[integer_text_capacity];
    char numeric_scale_text[integer_text_capacity];
    char column_type_text[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    struct information_schema_character_metadata character_metadata = {0};
    const char *column_type = NULL;
    const char *column_default = NULL;
    const char *extra = NULL;
    const char *is_nullable = "NO";
    const char *values[information_schema_columns_column_count] = {
        "def",
        schema->name,
        table->name,
        column->name,
        ordinal_text,
        NULL,
        is_nullable,
        information_schema_data_type_for_descriptor(column),
        NULL,
        NULL,
        information_schema_numeric_precision_for_descriptor(column),
        information_schema_numeric_scale_for_descriptor(column),
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        NULL,
    };
    int rc = information_schema_format_i64(
        database,
        column->ordinal_position,
        ordinal_text,
        sizeof(ordinal_text)
    );

    if (column->is_nullable) {
        is_nullable = "YES";
        values[information_schema_columns_is_nullable_column] = is_nullable;
    }
    if (rc == MYLITE_OK) {
        rc = column_default_display_text(
            database,
            column,
            default_text,
            sizeof(default_text),
            &column_default
        );
    }
    if (rc == MYLITE_OK) {
        rc = show_column_type_text(
            database,
            column->logical_type,
            column_type_text,
            sizeof(column_type_text),
            &column_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_character_metadata_for_descriptor(
            database,
            table,
            column,
            character_length_text,
            sizeof(character_length_text),
            character_octet_text,
            sizeof(character_octet_text),
            &character_metadata
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_columns_numeric_metadata(
            database,
            column,
            numeric_precision_text,
            sizeof(numeric_precision_text),
            numeric_scale_text,
            sizeof(numeric_scale_text),
            values
        );
    }
    if (rc == MYLITE_OK &&
        (column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
         column_descriptor_is_timestamp(column))) {
        values[information_schema_columns_datetime_precision_column] = "0";
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    extra = column_extra_text(column);

    values[information_schema_columns_default_column] = column_default;
    values[information_schema_columns_character_maximum_length_column] =
        character_metadata.character_maximum_length;
    values[information_schema_columns_character_octet_length_column] =
        character_metadata.character_octet_length;
    values[information_schema_columns_character_set_name_column] =
        character_metadata.character_set_name;
    values[information_schema_columns_collation_name_column] = character_metadata.collation_name;
    values[information_schema_columns_column_type_column] = column_type;
    values[information_schema_columns_column_key_column] = mylite_execution_column_key_text(
        (struct loaded_index_info_span){.indexes = indexes, .count = index_count},
        primary_key,
        column
    );
    values[information_schema_columns_extra_column] = extra;
    values[information_schema_columns_column_comment_column] = column->comment;
    if (column->is_generated) {
        values[information_schema_columns_generation_expression_column] =
            column->generation_expression;
    }

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_st_geometry_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *geometry_type_name = NULL;
    const char *values[information_schema_st_geometry_columns_column_count] = {
        "def",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };

    if (!column_descriptor_is_spatial(column)) {
        return MYLITE_OK;
    }
    if (!spatial_logical_type_display_text(column->logical_type, &geometry_type_name)) {
        set_runtime_error(database, "unsupported spatial metadata descriptor");
        return MYLITE_ERROR;
    }

    values[information_schema_st_geometry_columns_schema_column] = schema->name;
    values[information_schema_st_geometry_columns_table_column] = table->name;
    values[information_schema_st_geometry_columns_column_name_column] = column->name;
    values[information_schema_st_geometry_columns_type_name_column] = geometry_type_name;

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_columns_extensions_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *values[information_schema_columns_extensions_column_count] = {
        "def",
        schema->name,
        table->name,
        column->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_views_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_view_descriptor view = {0};
    int rc = mylite_catalog_read_view_by_table_id(database, table->table_id, &view);
    const char *values[information_schema_views_column_count] = {
        "def",
        schema->name,
        table->name,
        view.view_definition,
        view.check_option,
        view.is_updatable,
        view.definer,
        view.security_type,
        view.character_set_client,
        view.collation_connection,
    };

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_view_table_usage_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_view_descriptor view = {0};
    int rc = mylite_catalog_read_view_by_table_id(database, table->table_id, &view);
    const char *values[information_schema_view_table_usage_column_count] = {
        "def",
        schema->name,
        table->name,
        "def",
        view.source_schema_name,
        view.source_table_name,
    };

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int column_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    *out_default_text = NULL;
    if (column->is_generated) {
        return MYLITE_OK;
    }
    if (column->is_auto_increment) {
        return MYLITE_OK;
    }

    switch (column->default_kind) {
    case MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER:
        *out_default_text = default_text;
        return information_schema_format_i64(
            database,
            column->default_integer,
            default_text,
            default_text_size
        );
    case MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL:
    case MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION:
    case MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION:
        *out_default_text = column->default_text;
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_SCALAR_EXPRESSION:
        return format_scalar_expression_default_display_text(
            column,
            default_text,
            default_text_size,
            out_default_text
        );
    case MYLITE_CATALOG_COLUMN_DEFAULT_TEXT:
        if (column_descriptor_is_text_family(column)) {
            if (column->default_text[0] == '\0') {
                return MYLITE_OK;
            }
            int rc = format_mysql_utf8mb4_expression_text(
                column->default_text,
                default_text,
                default_text_size
            );

            if (rc == MYLITE_OK) {
                *out_default_text = default_text;
            }
            return rc;
        }
        *out_default_text = column->default_text;
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION: {
        int rc = format_mysql_utf8mb4_expression_text(
            column->default_text,
            default_text,
            default_text_size
        );

        if (rc == MYLITE_OK) {
            *out_default_text = default_text;
        }
        return rc;
    }
    case MYLITE_CATALOG_COLUMN_DEFAULT_BINARY:
        if (column_descriptor_is_binary_blob_family(column)) {
            return format_binary_blob_expression_default_display_text(
                database,
                column,
                default_text,
                default_text_size,
                out_default_text
            );
        }
        return format_binary_default_display_text(
            database,
            column,
            default_text,
            default_text_size,
            out_default_text
        );
    case MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP:
        *out_default_text = "CURRENT_TIMESTAMP";
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE:
        *out_default_text = "curdate()";
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME:
        *out_default_text = "curtime()";
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_NONE:
    case MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int format_scalar_expression_default_display_text(
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    if (text_has_ascii_case_insensitive_prefix(column->default_text, "concat(")) {
        return format_concat_scalar_expression_default_display_text(
            column->default_text,
            default_text,
            default_text_size,
            out_default_text
        );
    }

    *out_default_text = column->default_text;
    return MYLITE_OK;
}

static int format_concat_scalar_expression_default_display_text(
    const char *source,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    size_t write_index = 0U;

    if (source == NULL || default_text_size == 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t read_index = 0U; source[read_index] != '\0'; ++read_index) {
        if (source[read_index] == '\'') {
            if (write_index + 1U >= default_text_size) {
                return MYLITE_NOMEM;
            }
            default_text[write_index++] = '\\';
        }
        if (write_index + 1U >= default_text_size) {
            return MYLITE_NOMEM;
        }
        default_text[write_index++] = source[read_index];
    }
    default_text[write_index] = '\0';
    *out_default_text = default_text;
    return MYLITE_OK;
}

static int format_binary_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    size_t display_byte_count = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    display_byte_count = binary_default_display_byte_count(bytes, byte_count);
    if (byte_count == 0U) {
        if (default_text_size == 0U) {
            free(bytes);
            return MYLITE_NOMEM;
        }
        default_text[0] = '\0';
    } else if (display_byte_count == 0U) {
        if (default_text_size < sizeof("0x")) {
            free(bytes);
            return MYLITE_NOMEM;
        }
        memcpy(default_text, "0x", sizeof("0x"));
    } else {
        char *hex_text = NULL;
        size_t hex_length = 0U;

        rc = mylite_execution_format_hex_bytes(
            database,
            (const unsigned char *)bytes,
            display_byte_count,
            &hex_text
        );
        if (rc == MYLITE_OK) {
            hex_length = strlen(hex_text);
            if ((hex_length + sizeof("0x")) > default_text_size) {
                rc = MYLITE_NOMEM;
            } else {
                default_text[0] = '0';
                default_text[1] = 'x';
                memcpy(default_text + 2U, hex_text, hex_length + 1U);
            }
        }
        free(hex_text);
    }
    if (rc == MYLITE_OK) {
        *out_default_text = default_text;
    }
    free(bytes);
    return rc;
}

static int format_binary_blob_expression_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    char *hex_text = NULL;
    size_t hex_length = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (byte_count == 0U) {
        if (sizeof("X''") > default_text_size) {
            rc = MYLITE_NOMEM;
        } else {
            memcpy(default_text, "X''", sizeof("X''"));
        }
    } else {
        rc = mylite_execution_format_hex_bytes(database, (const unsigned char *)bytes, byte_count, &hex_text);
        if (rc == MYLITE_OK) {
            hex_length = strlen(hex_text);
            if ((hex_length + sizeof("0x")) > default_text_size) {
                rc = MYLITE_NOMEM;
            } else {
                default_text[0] = '0';
                default_text[1] = 'x';
                memcpy(default_text + 2U, hex_text, hex_length + 1U);
            }
        }
    }
    if (rc == MYLITE_OK) {
        *out_default_text = default_text;
    }
    free(hex_text);
    free(bytes);
    return rc;
}

static int decode_binary_default_text(
    struct mylite_db *database,
    const char *default_text,
    char **out_bytes,
    size_t *out_byte_count
) {
    size_t hex_length = default_text == NULL ? 0U : strlen(default_text);
    size_t byte_count = hex_length / 2U;
    char *bytes = NULL;

    if (default_text == NULL || out_bytes == NULL || out_byte_count == NULL ||
        (hex_length % 2U) != 0U) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;

    bytes = malloc(byte_count + 1U);
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t byte_index = 0U; byte_index < byte_count; ++byte_index) {
        unsigned char high = 0U;
        unsigned char low = 0U;
        size_t hex_index = byte_index * 2U;

        if (hex_nibble_value(default_text[hex_index], &high) != MYLITE_OK ||
            hex_nibble_value(default_text[hex_index + 1U], &low) != MYLITE_OK) {
            free(bytes);
            return MYLITE_MISUSE;
        }
        bytes[byte_index] = (char)((high << byte_high_nibble_shift) | low);
    }
    bytes[byte_count] = '\0';
    *out_bytes = bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static size_t binary_default_display_byte_count(const char *bytes, size_t byte_count) {
    for (size_t index = 0U; index < byte_count; ++index) {
        if (bytes[index] == '\0') {
            return index;
        }
    }
    return byte_count;
}

static int append_mysql_utf8mb4_expression_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = mylite_dynamic_string_append(string, "_utf8mb4'");

    if (rc == MYLITE_OK) {
        rc = append_mysql_escaped_expression_text(string, text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }
    return rc;
}

static int format_mysql_utf8mb4_expression_text(
    const char *text,
    char *buffer,
    size_t buffer_size
) {
    struct mylite_dynamic_string string = {0};
    int rc = append_mysql_utf8mb4_expression_text(&string, text);

    if (rc == MYLITE_OK && (string.length + 1U) > buffer_size) {
        rc = MYLITE_NOMEM;
    }
    if (rc == MYLITE_OK) {
        memcpy(buffer, string.text, string.length + 1U);
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_escaped_expression_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        switch (text[index]) {
        case '\'':
            rc = mylite_dynamic_string_append(string, "\\'");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }

    return rc;
}

static const char *column_extra_text(const struct mylite_catalog_column_descriptor *column) {
    bool generated = column_default_is_generated_extra(column);

    if (column->is_generated && column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED &&
        !column->is_visible) {
        return "STORED GENERATED INVISIBLE";
    }
    if (column->is_generated && column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED) {
        return "STORED GENERATED";
    }
    if (column->is_generated && !column->is_visible) {
        return "VIRTUAL GENERATED INVISIBLE";
    }
    if (column->is_generated) {
        return "VIRTUAL GENERATED";
    }
    if (column->is_auto_increment && !column->is_visible) {
        return "auto_increment INVISIBLE";
    }
    if (column->is_auto_increment) {
        return "auto_increment";
    }
    if (generated && column->on_update_current_timestamp && !column->is_visible) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (generated && column->on_update_current_timestamp) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP";
    }
    if (generated && !column->is_visible) {
        return "DEFAULT_GENERATED INVISIBLE";
    }
    if (generated) {
        return "DEFAULT_GENERATED";
    }
    if (column->on_update_current_timestamp && !column->is_visible) {
        return "on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (column->on_update_current_timestamp) {
        return "on update CURRENT_TIMESTAMP";
    }
    if (!column->is_visible) {
        return "INVISIBLE";
    }
    return "";
}

static bool column_default_is_generated_extra(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_default_kind_is_expression(column->default_kind) ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            (column_descriptor_is_text_family(column) &&
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) ||
            (column_descriptor_is_binary_blob_family(column) &&
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) != 0;
}

static int append_information_schema_columns_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *numeric_precision_text,
    size_t numeric_precision_text_size,
    char *numeric_scale_text,
    size_t numeric_scale_text_size,
    const char **values
) {
    const struct information_schema_columns_numeric_metadata_buffers buffers = {
        .numeric_precision_text = numeric_precision_text,
        .numeric_precision_text_size = numeric_precision_text_size,
        .numeric_scale_text = numeric_scale_text,
        .numeric_scale_text_size = numeric_scale_text_size,
        .values = values,
    };

    if (column_descriptor_is_decimal(column)) {
        return append_information_schema_columns_decimal_numeric_metadata(
            database,
            column,
            &buffers
        );
    }
    if (column_descriptor_is_bit(column)) {
        return append_information_schema_columns_bit_numeric_metadata(database, column, &buffers);
    }
    if (column_descriptor_is_approximate(column)) {
        return append_information_schema_columns_approximate_numeric_metadata(
            database,
            column,
            &buffers
        );
    }

    return MYLITE_OK;
}

static int append_information_schema_columns_decimal_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct information_schema_columns_numeric_metadata_buffers *buffers
) {
    struct decimal_type_info info = {0};
    int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)info.precision,
            buffers->numeric_precision_text,
            buffers->numeric_precision_text_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)info.scale,
            buffers->numeric_scale_text,
            buffers->numeric_scale_text_size
        );
    }
    if (rc == MYLITE_OK) {
        buffers->values[information_schema_columns_numeric_precision_column] =
            buffers->numeric_precision_text;
        buffers->values[information_schema_columns_numeric_scale_column] =
            buffers->numeric_scale_text;
    }
    return rc;
}

static int append_information_schema_columns_bit_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct information_schema_columns_numeric_metadata_buffers *buffers
) {
    uint64_t width = 0U;
    int rc = bit_width_for_logical_type(column->logical_type, &width);

    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)width,
            buffers->numeric_precision_text,
            buffers->numeric_precision_text_size
        );
    }
    if (rc == MYLITE_OK) {
        buffers->values[information_schema_columns_numeric_precision_column] =
            buffers->numeric_precision_text;
        buffers->values[information_schema_columns_numeric_scale_column] = NULL;
    }
    return rc;
}

static int append_information_schema_columns_approximate_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct information_schema_columns_numeric_metadata_buffers *buffers
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info.has_scale) {
        rc = information_schema_format_i64(
            database,
            (int64_t)info.precision,
            buffers->numeric_precision_text,
            buffers->numeric_precision_text_size
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)info.scale,
                buffers->numeric_scale_text,
                buffers->numeric_scale_text_size
            );
        }
        if (rc == MYLITE_OK) {
            buffers->values[information_schema_columns_numeric_precision_column] =
                buffers->numeric_precision_text;
            buffers->values[information_schema_columns_numeric_scale_column] =
                buffers->numeric_scale_text;
        }
    } else {
        buffers->values[information_schema_columns_numeric_precision_column] =
            info.type_class == APPROXIMATE_TYPE_FLOAT ? "12" : "22";
        buffers->values[information_schema_columns_numeric_scale_column] = NULL;
    }
    return rc;
}
