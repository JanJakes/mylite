static int append_information_schema_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_information_schema_table_definition_at(table_index)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_columns_mysql_system_rows(database, rows);
    }
    return rc;
}

static int append_information_schema_columns_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->columns[column_index];
        char ordinal_text[integer_text_capacity];
        const char *values[information_schema_columns_column_count] = {
            "def",
            "information_schema",
            definition->name,
            column->name,
            ordinal_text,
            column->column_default,
            column->is_nullable,
            column->data_type,
            column->character_maximum_length,
            column->character_octet_length,
            column->numeric_precision,
            column->numeric_scale,
            column->datetime_precision,
            column->character_set_name,
            column->collation_name,
            column->column_type,
            "",
            "",
            "select",
            "",
            "",
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)column_index + 1,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_information_schema_columns_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(table_index)
        );
    }
    return rc;
}

static int append_information_schema_columns_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < definition->query_definition.column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[column_index];
        char ordinal_text[integer_text_capacity];
        const char *values[information_schema_columns_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            ordinal_text,
            column->column_default,
            column->is_nullable,
            column->data_type,
            column->character_maximum_length,
            column->character_octet_length,
            column->numeric_precision,
            column->numeric_scale,
            column->datetime_precision,
            column->character_set_name,
            column->collation_name,
            column->column_type,
            definition->column_keys[column_index],
            definition->column_extras[column_index],
            definition->column_privileges[column_index],
            mysql_system_table_column_comment(definition, column_index),
            mysql_system_table_column_generation_expression(definition, column_index),
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)column_index + 1,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_information_schema_columns_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_extensions_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_information_schema_table_definition_at(table_index)
        );
    }
    return rc;
}

static int append_information_schema_columns_extensions_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->columns[column_index];
        const char *values[information_schema_columns_extensions_column_count] = {
            "def",
            "information_schema",
            definition->name,
            column->name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

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
    case MYLITE_CATALOG_COLUMN_DEFAULT_TEXT:
        if (column_descriptor_is_text_family(column)) {
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
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};
        int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)info.precision,
                numeric_precision_text,
                numeric_precision_text_size
            );
        }
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)info.scale,
                numeric_scale_text,
                numeric_scale_text_size
            );
        }
        if (rc == MYLITE_OK) {
            values[information_schema_columns_numeric_precision_column] = numeric_precision_text;
            values[information_schema_columns_numeric_scale_column] = numeric_scale_text;
        }
        return rc;
    }
    if (column_descriptor_is_bit(column)) {
        uint64_t width = 0U;
        int rc = bit_width_for_logical_type(column->logical_type, &width);

        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)width,
                numeric_precision_text,
                numeric_precision_text_size
            );
        }
        if (rc == MYLITE_OK) {
            values[information_schema_columns_numeric_precision_column] = numeric_precision_text;
            values[information_schema_columns_numeric_scale_column] = NULL;
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        struct approximate_type_info info = {
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };
        int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

        if (rc != MYLITE_OK) {
            return rc;
        }
        values[information_schema_columns_numeric_precision_column] =
            info.type_class == APPROXIMATE_TYPE_FLOAT ? "12" : "22";
        values[information_schema_columns_numeric_scale_column] = NULL;
    }

    return MYLITE_OK;
}

static int append_information_schema_innodb_virtual_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t virtual_index = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_virtual_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &columns[index];

        if (!column->is_generated ||
            column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL) {
            continue;
        }
        rc = append_information_schema_innodb_virtual_column_rows(
            database,
            rows,
            table,
            columns,
            column_count,
            column,
            virtual_index
        );
        ++virtual_index;
    }

    free(columns);
    return rc;
}

static int append_information_schema_innodb_virtual_column_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index
) {
    bool *base_positions = NULL;
    size_t base_position_count = 0U;
    int64_t position = 0;
    int rc = MYLITE_OK;

    if (column_count == 0U) {
        return MYLITE_OK;
    }
    base_positions = calloc(column_count, sizeof(*base_positions));
    if (base_positions == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = collect_information_schema_innodb_virtual_base_positions(
        database,
        columns,
        column_count,
        column,
        base_positions,
        &base_position_count
    );
    if (rc == MYLITE_OK && base_position_count != 0U) {
        rc = information_schema_innodb_virtual_position(database, column, virtual_index, &position);
    }
    for (size_t base_position = 0U; rc == MYLITE_OK && base_position < column_count;
         ++base_position) {
        if (!base_positions[base_position]) {
            continue;
        }
        if (base_position > (size_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base position");
            rc = MYLITE_ERROR;
            break;
        }
        rc = append_information_schema_innodb_virtual_dependency_row(
            database,
            rows,
            table->table_id,
            position,
            (int64_t)base_position
        );
    }

    free(base_positions);
    return rc;
}

static int collect_information_schema_innodb_virtual_base_positions(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    bool *base_positions,
    size_t *out_base_position_count
) {
    const char *expression = column == NULL ? NULL : column->generation_expression;
    size_t expression_index = 0U;
    int rc = MYLITE_OK;

    *out_base_position_count = 0U;
    if (expression == NULL || expression[0] == '\0') {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL expression");
        return MYLITE_ERROR;
    }

    while (rc == MYLITE_OK && expression[expression_index] != '\0') {
        char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        if (expression[expression_index] != '`') {
            ++expression_index;
            continue;
        }
        rc = copy_information_schema_innodb_virtual_identifier(
            database,
            expression,
            &expression_index,
            identifier,
            sizeof(identifier)
        );
        if (rc == MYLITE_OK) {
            rc = collect_information_schema_innodb_virtual_base_identifier(
                database,
                columns,
                column_count,
                identifier,
                base_positions,
                out_base_position_count
            );
        }
    }

    return rc;
}

static int collect_information_schema_innodb_virtual_base_identifier(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    bool *base_positions,
    size_t *base_position_count
) {
    const struct mylite_catalog_column_descriptor *base_column = NULL;
    int64_t base_position = 0;
    int rc = find_information_schema_innodb_virtual_base_column(
        columns,
        column_count,
        identifier,
        &base_column
    );

    if (rc != MYLITE_OK || base_column == NULL || base_column->is_generated) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base column");
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_position(database, base_column, &base_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (base_position < 0 || (uint64_t)base_position >= column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base position");
        return MYLITE_ERROR;
    }
    if (!base_positions[base_position]) {
        base_positions[base_position] = true;
        ++(*base_position_count);
    }
    return MYLITE_OK;
}

static int copy_information_schema_innodb_virtual_identifier(
    struct mylite_db *database,
    const char *expression,
    size_t *expression_index,
    char *identifier,
    size_t identifier_size
) {
    size_t identifier_index = 0U;
    size_t index = *expression_index + 1U;

    while (expression[index] != '\0') {
        if (expression[index] == '`') {
            if (expression[index + 1U] == '`') {
                if (identifier_index + 1U >= identifier_size) {
                    set_runtime_error(
                        database,
                        "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier"
                    );
                    return MYLITE_ERROR;
                }
                identifier[identifier_index] = '`';
                ++identifier_index;
                index += 2U;
                continue;
            }
            if (identifier_index == 0U) {
                set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
                return MYLITE_ERROR;
            }
            identifier[identifier_index] = '\0';
            *expression_index = index + 1U;
            return MYLITE_OK;
        }
        if (identifier_index + 1U >= identifier_size) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
            return MYLITE_ERROR;
        }
        identifier[identifier_index] = expression[index];
        ++identifier_index;
        ++index;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
    return MYLITE_ERROR;
}

static int find_information_schema_innodb_virtual_base_column(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    const struct mylite_catalog_column_descriptor **out_column
) {
    *out_column = NULL;
    for (size_t index = 0U; index < column_count; ++index) {
        if (text_equals_ascii_case_insensitive(columns[index].name, identifier)) {
            *out_column = &columns[index];
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static int information_schema_innodb_virtual_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index,
    int64_t *out_position
) {
    const int64_t virtual_position_shift = 65536;
    int64_t column_position = 0;
    uint64_t virtual_number = (uint64_t)virtual_index + 1U;
    int rc = information_schema_innodb_column_position(database, column, &column_position);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_position < 0 ||
        virtual_number > (uint64_t)((INT64_MAX - column_position) / virtual_position_shift)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL position");
        return MYLITE_ERROR;
    }

    *out_position = (int64_t)(virtual_number * (uint64_t)virtual_position_shift) + column_position;
    return MYLITE_OK;
}

static int append_information_schema_innodb_virtual_dependency_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    int64_t table_id,
    int64_t position,
    int64_t base_position
) {
    char table_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    char base_position_text[integer_text_capacity];
    int rc =
        information_schema_format_i64(database, table_id, table_id_text, sizeof(table_id_text));

    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, position, position_text, sizeof(position_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            base_position,
            base_position_text,
            sizeof(base_position_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_virtual_column_count] = {
            table_id_text,
            position_text,
            base_position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_columns_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        rc = append_information_schema_innodb_columns_column_row(
            database,
            rows,
            table,
            &columns[index]
        );
    }

    free(columns);
    return rc;
}

static int append_information_schema_innodb_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    struct information_schema_innodb_column_type_info type_info = {0};
    char table_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    char mtype_text[integer_text_capacity];
    char prtype_text[integer_text_capacity];
    char length_text[integer_text_capacity];
    char has_default_text[integer_text_capacity];
    int64_t position = 0;
    int rc = MYLITE_OK;

    rc = information_schema_innodb_column_position(database, column, &position);
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_type_info(database, table, column, &type_info);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, position, position_text, sizeof(position_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.mtype,
            mtype_text,
            sizeof(mtype_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.prtype,
            prtype_text,
            sizeof(prtype_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.length,
            length_text,
            sizeof(length_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_column_has_default,
            has_default_text,
            sizeof(has_default_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_columns_column_count] = {
            table_id_text,
            column->name,
            position_text,
            mtype_text,
            prtype_text,
            length_text,
            has_default_text,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int information_schema_innodb_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_position
) {
    if (column == NULL || out_position == NULL || column->ordinal_position <= 0) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS position");
        return MYLITE_ERROR;
    }

    *out_position = column->ordinal_position - 1;
    return MYLITE_OK;
}

static int information_schema_innodb_column_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    if (out_info == NULL || column == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column input");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        return information_schema_innodb_column_character_type_info(
            database,
            table,
            column,
            (struct information_schema_innodb_character_type_request){
                .mtype = information_schema_innodb_column_mtype_char,
                .mysql_type_code = information_schema_innodb_column_mysql_type_string,
            },
            out_info
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return information_schema_innodb_column_character_type_info(
            database,
            table,
            column,
            (struct information_schema_innodb_character_type_request){
                .mtype = information_schema_innodb_column_mtype_varchar,
                .mysql_type_code = information_schema_innodb_column_mysql_type_varchar,
            },
            out_info
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return information_schema_innodb_column_text_family_type_info(
            database,
            table,
            column,
            out_info
        );
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return information_schema_innodb_column_binary_type_info(database, column, out_info);
    }

    return information_schema_innodb_column_nonstring_type_info(database, column, out_info);
}

static int information_schema_innodb_column_nonstring_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    if (column_descriptor_is_json(column)) {
        int64_t prtype = 0;

        int rc = information_schema_innodb_column_collation_type_prtype(
            database,
            "utf8mb4_bin",
            information_schema_innodb_column_numeric_flag +
                information_schema_innodb_column_mysql_type_json,
            column->is_nullable,
            &prtype
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_blob,
            .prtype = prtype,
            .length = information_schema_innodb_column_blob_length_long,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_spatial(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_geometry,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_mysql_type_geometry +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = information_schema_innodb_column_blob_length_long,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_bit(column)) {
        return information_schema_innodb_column_bit_type_info(database, column, out_info);
    }
    if (column_descriptor_is_decimal(column)) {
        return information_schema_innodb_column_decimal_type_info(database, column, out_info);
    }
    if (column_descriptor_is_approximate(column)) {
        return information_schema_innodb_column_approximate_type_info(database, column, out_info);
    }
    if (column_descriptor_is_year(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_int,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_unsigned_flag +
                      information_schema_innodb_column_mysql_type_year +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = year_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_date(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_int,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_mysql_type_date +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = date_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_time(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_time +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = time_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_datetime +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = datetime_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_timestamp(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_timestamp +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = timestamp_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_enum(column)) {
        return information_schema_innodb_column_enum_type_info(database, column, out_info);
    }
    if (column_descriptor_is_set(column)) {
        return information_schema_innodb_column_set_type_info(database, column, out_info);
    }

    return information_schema_innodb_column_integer_type_info(database, column, out_info);
}

static int information_schema_innodb_column_character_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_character_type_request request,
    struct information_schema_innodb_column_type_info *out_info
) {
    size_t character_count = 0U;
    uint64_t length = 0U;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (request.mtype == information_schema_innodb_column_mtype_char) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline CHAR descriptors",
            &character_count
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline VARCHAR descriptors",
            &character_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    length = (uint64_t)character_count * column_effective_max_bytes_per_character(table, column);
    if (length > (uint64_t)INT64_MAX) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_collation_prtype(
        database,
        column_effective_collation_name(table, column),
        request.mysql_type_code,
        column->is_nullable,
        &prtype
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = request.mtype,
        .prtype = prtype,
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_text_family_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    const struct text_family_type_info *text_info =
        text_family_type_info_for_logical_type(column->logical_type);
    int64_t length = 0;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (text_info == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline TEXT descriptors"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_blob_family_length(
        database,
        text_info->maximum_length,
        &length
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_collation_prtype(
            database,
            column_effective_collation_name(table, column),
            information_schema_innodb_column_mysql_type_blob,
            column->is_nullable,
            &prtype
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_blob,
        .prtype = prtype,
        .length = length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_binary_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *binary_info =
        binary_string_type_info_for_logical_type(column->logical_type, &storage);
    int64_t length = 0;
    int64_t mtype = information_schema_innodb_column_mtype_blob;
    int64_t mysql_type_code = information_schema_innodb_column_mysql_type_blob;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (binary_info == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline binary descriptors"
        );
        return MYLITE_ERROR;
    }
    if (binary_info->blob_family) {
        rc = information_schema_innodb_column_blob_family_length(
            database,
            binary_info->maximum_length,
            &length
        );
    } else {
        if (binary_info->maximum_length > (uint64_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
            return MYLITE_ERROR;
        }
        length = (int64_t)binary_info->maximum_length;
        if (binary_info->fixed_length) {
            mtype = information_schema_innodb_column_mtype_fixbinary;
            mysql_type_code = information_schema_innodb_column_mysql_type_string;
        } else {
            mtype = information_schema_innodb_column_mtype_binary;
            mysql_type_code = information_schema_innodb_column_mysql_type_varchar;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_binary_prtype(
            database,
            mysql_type_code,
            column->is_nullable,
            &prtype
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = mtype,
        .prtype = prtype,
        .length = length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_integer_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t length = 0U;
    int64_t mysql_type_code = 0;
    int rc = information_schema_innodb_column_integer_mysql_type(
        database,
        column->logical_type,
        &mysql_type_code
    );

    if (rc == MYLITE_OK) {
        rc = integer_row_size_bytes(column->logical_type, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline integer descriptors"
        );
        return MYLITE_ERROR;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_numeric_flag + mysql_type_code +
                  (information_schema_innodb_column_logical_type_is_unsigned(column->logical_type)
                       ? information_schema_innodb_column_unsigned_flag
                       : 0) +
                  (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_decimal_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct decimal_type_info decimal_info = {0};
    uint64_t length = 0U;
    int rc = decimal_type_info_for_logical_type(column->logical_type, &decimal_info);

    if (rc == MYLITE_OK) {
        rc = decimal_row_size_bytes(&decimal_info, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline DECIMAL descriptors"
        );
        return MYLITE_ERROR;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_fixbinary,
        .prtype = information_schema_innodb_column_temporal_decimal_flag +
                  information_schema_innodb_column_mysql_type_newdecimal +
                  (decimal_info.is_unsigned ? information_schema_innodb_column_unsigned_flag : 0) +
                  (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_approximate_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t length = 0U;
    int64_t mtype = information_schema_innodb_column_mtype_double;
    int64_t mysql_type_code = information_schema_innodb_column_mysql_type_double;
    int rc = approximate_type_info_for_logical_type(column->logical_type, &approximate_info);

    if (rc == MYLITE_OK) {
        rc = approximate_row_size_bytes(&approximate_info, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline approximate descriptors"
        );
        return MYLITE_ERROR;
    }
    if (approximate_info.type_class == APPROXIMATE_TYPE_FLOAT) {
        mtype = information_schema_innodb_column_mtype_float;
        mysql_type_code = information_schema_innodb_column_mysql_type_float;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = mtype,
        .prtype =
            information_schema_innodb_column_numeric_flag + mysql_type_code +
            (approximate_info.is_unsigned ? information_schema_innodb_column_unsigned_flag : 0) +
            (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_bit_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t width = 0U;
    int64_t prtype = 0;
    int rc = bit_width_for_logical_type(column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline BIT descriptors"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_collation_type_prtype(
        database,
        "binary",
        information_schema_innodb_column_numeric_flag +
            information_schema_innodb_column_unsigned_flag +
            information_schema_innodb_column_mysql_type_bit,
        column->is_nullable,
        &prtype
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_fixbinary,
        .prtype = prtype,
        .length = (int64_t)bit_byte_width(width),
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_enum_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct enum_type_info enum_info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline ENUM descriptors",
        &enum_info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_unsigned_flag +
                  information_schema_innodb_column_mysql_type_string +
                  information_schema_innodb_column_not_null_prtype_flag(column),
        .length = enum_info.label_count <= enum_one_byte_label_count_max ? 1 : 2,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_set_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t length = 0U;
    int rc = set_row_size_bytes(
        database,
        column->logical_type,
        &length,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline SET descriptors"
    );

    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        if (rc == MYLITE_OK) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
            return MYLITE_ERROR;
        }
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_unsigned_flag +
                  information_schema_innodb_column_mysql_type_string +
                  information_schema_innodb_column_not_null_prtype_flag(column),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_blob_family_length(
    struct mylite_db *database,
    uint64_t maximum_length,
    int64_t *out_length
) {
    if (maximum_length == tinytext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_tiny;
        return MYLITE_OK;
    }
    if (maximum_length == text_max_length) {
        *out_length = information_schema_innodb_column_blob_length_regular;
        return MYLITE_OK;
    }
    if (maximum_length == mediumtext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_medium;
        return MYLITE_OK;
    }
    if (maximum_length == longtext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_long;
        return MYLITE_OK;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS blob length");
    return MYLITE_ERROR;
}

static int information_schema_innodb_column_collation_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
) {
    return information_schema_innodb_column_collation_type_prtype(
        database,
        collation_name,
        mysql_type_code,
        is_nullable,
        out_prtype
    );
}

static int information_schema_innodb_column_binary_prtype(
    struct mylite_db *database,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
) {
    return information_schema_innodb_column_collation_type_prtype(
        database,
        "binary",
        information_schema_innodb_column_numeric_flag + mysql_type_code,
        is_nullable,
        out_prtype
    );
}

static int information_schema_innodb_column_collation_type_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t type_flags,
    bool is_nullable,
    int64_t *out_prtype
) {
    uint32_t collation_id = result_metadata_collation_id(collation_name);
    int64_t nullability_flag = is_nullable ? 0 : information_schema_innodb_column_not_null_flag;
    int64_t collation_part = 0;

    if (out_prtype == NULL || type_flags < 0 ||
        collation_id > (uint32_t)(INT64_MAX >> information_schema_innodb_column_collation_shift)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS prtype");
        return MYLITE_ERROR;
    }

    collation_part = (int64_t)collation_id << information_schema_innodb_column_collation_shift;
    if (type_flags > INT64_MAX - nullability_flag ||
        collation_part > INT64_MAX - type_flags - nullability_flag) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS prtype");
        return MYLITE_ERROR;
    }

    *out_prtype = collation_part + type_flags + nullability_flag;
    return MYLITE_OK;
}

static int information_schema_innodb_column_integer_mysql_type(
    struct mylite_db *database,
    const char *logical_type,
    int64_t *out_type_code
) {
    if (logical_type == NULL || out_type_code == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS integer type");
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_tiny;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_short;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_int24;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_longlong;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_long;
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline integer descriptors"
    );
    return MYLITE_ERROR;
}

static int64_t information_schema_innodb_column_not_null_prtype_flag(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && !column->is_nullable) {
        return information_schema_innodb_column_not_null_flag;
    }
    return 0;
}

static bool information_schema_innodb_column_logical_type_is_unsigned(const char *logical_type) {
    static const char suffix[] = " UNSIGNED";
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t suffix_length = sizeof(suffix) - 1U;

    return (logical_length >= suffix_length &&
            strcmp(&logical_type[logical_length - suffix_length], suffix) == 0) != 0;
}

static int append_information_schema_innodb_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct table_status_values status = {0};
    char *table_name = NULL;
    char table_id_text[integer_text_capacity];
    char flag_text[integer_text_capacity];
    char column_count_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char zip_page_size_text[integer_text_capacity];
    char instant_cols_text[integer_text_capacity];
    char total_row_versions_text[integer_text_capacity];
    size_t column_count = 0U;
    int64_t flag = 0;
    int64_t innodb_column_count = 0;
    int64_t zip_page_size = 0;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tables_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_status_row_format(database, table, &status);
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_schema_object_name(
            database,
            schema->name,
            table->name,
            &table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_flag(database, status.row_format, &flag);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_column_count(
            database,
            column_count,
            &innodb_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_zip_page_size(
            database,
            table,
            status.row_format,
            &zip_page_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, flag, flag_text, sizeof(flag_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            innodb_column_count,
            column_count_text,
            sizeof(column_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            zip_page_size,
            zip_page_size_text,
            sizeof(zip_page_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_instant_cols,
            instant_cols_text,
            sizeof(instant_cols_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_total_row_versions,
            total_row_versions_text,
            sizeof(total_row_versions_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_tables_column_count] = {
            table_id_text,
            table_name,
            flag_text,
            column_count_text,
            space_text,
            status.row_format,
            zip_page_size_text,
            "Single",
            instant_cols_text,
            total_row_versions_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(table_name);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_tablestats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char *table_name = NULL;
    char table_id_text[integer_text_capacity];
    char row_count_text[integer_text_capacity];
    char other_index_size_text[integer_text_capacity];
    char autoinc_text[integer_text_capacity];
    int64_t row_count = 0;
    int64_t other_index_size = 0;
    int64_t autoinc = 0;
    bool has_auto_increment = false;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tablestats_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESTATS columns");
        return MYLITE_ERROR;
    }

    rc = copy_information_schema_schema_object_name(
        database,
        schema->name,
        table->name,
        &table_name
    );
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read INFORMATION_SCHEMA.INNODB_TABLESTATS rows");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_count_nonprimary_indexes(
            database,
            table->table_id,
            &other_index_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = table_status_has_auto_increment(database, table, &has_auto_increment);
    }
    if (rc == MYLITE_OK && has_auto_increment) {
        autoinc = table->auto_increment_next;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            row_count,
            row_count_text,
            sizeof(row_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            other_index_size,
            other_index_size_text,
            sizeof(other_index_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, autoinc, autoinc_text, sizeof(autoinc_text));
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_tablestats_column_count] = {
            table_id_text,
            table_name,
            "Initialized",
            row_count_text,
            "1",
            other_index_size_text,
            "0",
            autoinc_text,
            "1",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(table_name);
    return rc;
}

static int information_schema_count_nonprimary_indexes(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_count
) {
    struct nonprimary_index_count_context context = {.count = 0};
    int rc = MYLITE_OK;

    if (out_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0;

    rc = mylite_catalog_for_each_index_in_table(
        database,
        table_id,
        count_nonprimary_index,
        &context
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.INNODB_TABLESTATS indexes");
        return MYLITE_ERROR;
    }
    *out_count = context.count;
    return MYLITE_OK;
}

static int count_nonprimary_index(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct nonprimary_index_count_context *context = user_data;

    if (context == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        if (context->count == INT64_MAX) {
            return MYLITE_ERROR;
        }
        ++context->count;
    }
    return MYLITE_OK;
}

static int information_schema_innodb_table_flag(
    struct mylite_db *database,
    const char *row_format,
    int64_t *out_flag
) {
    if (text_equals_ascii_case_insensitive(row_format, "Redundant")) {
        *out_flag = information_schema_innodb_table_redundant_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Compact")) {
        *out_flag = information_schema_innodb_table_compact_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Dynamic")) {
        *out_flag = information_schema_innodb_table_dynamic_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Compressed")) {
        *out_flag = information_schema_innodb_table_compressed_flag;
        return MYLITE_OK;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES row format");
    return MYLITE_ERROR;
}

static int information_schema_innodb_table_column_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_column_count
) {
    if (column_count > (size_t)INT64_MAX - information_schema_innodb_table_hidden_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES column count");
        return MYLITE_ERROR;
    }

    *out_column_count =
        (int64_t)(column_count + information_schema_innodb_table_hidden_column_count);
    return MYLITE_OK;
}

static int information_schema_innodb_table_zip_page_size(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const char *row_format,
    int64_t *out_zip_page_size
) {
    if (!text_equals_ascii_case_insensitive(row_format, "Compressed")) {
        *out_zip_page_size = information_schema_innodb_table_default_zip_page_size;
        return MYLITE_OK;
    }
    if (table->key_block_size < 0) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES zip page size");
        return MYLITE_ERROR;
    }
    if (table->key_block_size == 0) {
        *out_zip_page_size = information_schema_innodb_table_default_compressed_zip_page_size;
        return MYLITE_OK;
    }
    if (table->key_block_size > INT64_MAX / information_schema_innodb_table_zip_page_unit) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES zip page size");
        return MYLITE_ERROR;
    }

    *out_zip_page_size = table->key_block_size * information_schema_innodb_table_zip_page_unit;
    return MYLITE_OK;
}

static int append_information_schema_innodb_indexes_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *clustered_index = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_indexes_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
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
    if (rc == MYLITE_OK) {
        clustered_index = information_schema_innodb_clustered_index(indexes, index_count);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_innodb_index_row(
            database,
            rows,
            table,
            column_count,
            clustered_index,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK && clustered_index == NULL) {
        rc = append_information_schema_innodb_generated_cluster_index_row(
            database,
            rows,
            table,
            column_count
        );
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
) {
    char index_id_text[integer_text_capacity];
    char table_id_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    char field_count_text[integer_text_capacity];
    char page_no_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char merge_threshold_text[integer_text_capacity];
    int64_t type = 0;
    int64_t field_count = 0;
    int64_t page_no = information_schema_innodb_index_default_page_no;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        page_no = information_schema_innodb_index_fulltext_page_no;
    }

    rc = information_schema_format_i64(
        database,
        index->index.index_id,
        index_id_text,
        sizeof(index_id_text)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_index_type(database, index, clustered_index, &type);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, type, type_text, sizeof(type_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_index_field_count(
            database,
            index,
            column_count,
            clustered_index,
            &field_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            field_count,
            field_count_text,
            sizeof(field_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, page_no, page_no_text, sizeof(page_no_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_merge_threshold,
            merge_threshold_text,
            sizeof(merge_threshold_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_indexes_column_count] = {
            index_id_text,
            index->index.name,
            table_id_text,
            type_text,
            field_count_text,
            page_no_text,
            space_text,
            merge_threshold_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_innodb_generated_cluster_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count
) {
    char index_id_text[integer_text_capacity];
    char table_id_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    char field_count_text[integer_text_capacity];
    char page_no_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char merge_threshold_text[integer_text_capacity];
    int64_t index_id = 0;
    int64_t field_count = 0;
    int rc =
        information_schema_innodb_generated_cluster_index_id(database, table->table_id, &index_id);

    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_generated_cluster_field_count(
            database,
            column_count,
            &field_count
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, index_id, index_id_text, sizeof(index_id_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_generated_cluster_type,
            type_text,
            sizeof(type_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            field_count,
            field_count_text,
            sizeof(field_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_default_page_no,
            page_no_text,
            sizeof(page_no_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_merge_threshold,
            merge_threshold_text,
            sizeof(merge_threshold_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_indexes_column_count] = {
            index_id_text,
            information_schema_innodb_generated_cluster_index_name,
            table_id_text,
            type_text,
            field_count_text,
            page_no_text,
            space_text,
            merge_threshold_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_innodb_fields_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_fields_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FIELDS columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
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
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_innodb_fields_index_rows(database, rows, &indexes[index]);
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_fields_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        rc = append_information_schema_innodb_fields_part_row(
            database,
            rows,
            index,
            &index->parts[part_index]
        );
    }
    return rc;
}

static int append_information_schema_innodb_fields_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
) {
    char index_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (part->index_column.ordinal_position < information_schema_innodb_field_position_offset) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FIELDS position");
        return MYLITE_ERROR;
    }

    rc = information_schema_format_i64(
        database,
        index->index.index_id,
        index_id_text,
        sizeof(index_id_text)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position - information_schema_innodb_field_position_offset,
            position_text,
            sizeof(position_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_fields_column_count] = {
            index_id_text,
            part->column.name,
            position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int information_schema_innodb_index_type(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    int64_t *out_type
) {
    switch (index->index.kind) {
    case MYLITE_CATALOG_INDEX_KIND_PRIMARY:
        *out_type = information_schema_innodb_index_primary_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_SECONDARY:
        if (index == clustered_index) {
            *out_type = information_schema_innodb_index_primary_type;
        } else {
            *out_type = index->index.is_unique ? information_schema_innodb_index_unique_type
                                               : information_schema_innodb_index_nonunique_type;
        }
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_FULLTEXT:
        *out_type = information_schema_innodb_index_fulltext_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_SPATIAL:
        *out_type = information_schema_innodb_index_spatial_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_INVALID:
        break;
    }

    set_runtime_error(database, "unknown INFORMATION_SCHEMA.INNODB_INDEXES index type");
    return MYLITE_ERROR;
}

static int information_schema_innodb_index_field_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    int64_t *out_field_count
) {
    size_t clustered_part_count = 0U;
    size_t field_count = index->part_count;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        if (index->part_count > (size_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        *out_field_count = (int64_t)index->part_count;
        return MYLITE_OK;
    }
    if (index == clustered_index) {
        if (column_count >
            (size_t)INT64_MAX - information_schema_innodb_cluster_system_field_count) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        *out_field_count =
            (int64_t)(column_count + information_schema_innodb_cluster_system_field_count);
        return MYLITE_OK;
    }

    rc = information_schema_innodb_secondary_clustered_part_count(
        database,
        index,
        clustered_index,
        &clustered_part_count
    );
    if (rc == MYLITE_OK) {
        if (field_count > (size_t)INT64_MAX - clustered_part_count) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        field_count += clustered_part_count;
        *out_field_count = (int64_t)field_count;
    }
    return rc;
}

static int information_schema_innodb_secondary_clustered_part_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    size_t *out_count
) {
    size_t count = 0U;

    if (clustered_index == NULL) {
        *out_count = information_schema_innodb_generated_cluster_key_part_count;
        return MYLITE_OK;
    }

    for (size_t part_index = 0U; part_index < clustered_index->part_count; ++part_index) {
        if (!information_schema_innodb_index_contains_column(
                index,
                clustered_index->parts[part_index].column.column_id
            )) {
            if (count == SIZE_MAX) {
                set_runtime_error(
                    database,
                    "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count"
                );
                return MYLITE_ERROR;
            }
            ++count;
        }
    }

    *out_count = count;
    return MYLITE_OK;
}

static int information_schema_innodb_generated_cluster_field_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_field_count
) {
    if (column_count >
        (size_t)INT64_MAX - information_schema_innodb_generated_cluster_system_field_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
        return MYLITE_ERROR;
    }

    *out_field_count =
        (int64_t)(column_count + information_schema_innodb_generated_cluster_system_field_count);
    return MYLITE_OK;
}

static int information_schema_innodb_generated_cluster_index_id(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_index_id
) {
    if (table_id < 0 ||
        table_id > INT64_MAX - information_schema_innodb_generated_cluster_index_id_base) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES generated index id");
        return MYLITE_ERROR;
    }

    *out_index_id = information_schema_innodb_generated_cluster_index_id_base + table_id;
    return MYLITE_OK;
}

static const struct loaded_index_info *information_schema_innodb_clustered_index(
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            return &indexes[index];
        }
    }
    for (size_t index = 0U; index < index_count; ++index) {
        if (information_schema_innodb_index_is_all_not_null_unique(&indexes[index])) {
            return &indexes[index];
        }
    }

    return NULL;
}

static bool information_schema_innodb_index_is_all_not_null_unique(
    const struct loaded_index_info *index
) {
    if (index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY || !index->index.is_unique ||
        index->part_count == 0U) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.is_nullable) {
            return false;
        }
    }
    return true;
}

static bool information_schema_innodb_index_contains_column(
    const struct loaded_index_info *index,
    int64_t column_id
) {
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.column_id == column_id) {
            return true;
        }
    }
    return false;
}

static int append_information_schema_innodb_foreign_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_foreign_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_innodb_foreign_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_foreign_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    struct mylite_catalog_schema_descriptor parent_schema = {0};
    const struct mylite_catalog_schema_descriptor *reference_schema = schema;
    char *foreign_key_id = NULL;
    char *foreign_table_name = NULL;
    char *referenced_table_name = NULL;
    char column_count_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    int64_t type = 0;
    int rc = MYLITE_OK;

    if (foreign_key->part_count > (size_t)INT64_MAX) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN column count");
        return MYLITE_ERROR;
    }

    if (foreign_key->parent_table.schema_id != schema->schema_id) {
        rc = mylite_catalog_read_schema_by_id(
            database,
            foreign_key->parent_table.schema_id,
            &parent_schema
        );
        if (rc == MYLITE_OK) {
            reference_schema = &parent_schema;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_id(
            database,
            schema,
            foreign_key,
            &foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_table_name(
            database,
            schema,
            table,
            &foreign_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_table_name(
            database,
            reference_schema,
            &foreign_key->parent_table,
            &referenced_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)foreign_key->part_count,
            column_count_text,
            sizeof(column_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_foreign_type(database, &foreign_key->foreign_key, &type);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, type, type_text, sizeof(type_text));
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_foreign_column_count] = {
            foreign_key_id,
            foreign_table_name,
            referenced_table_name,
            column_count_text,
            type_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(referenced_table_name);
    free(foreign_table_name);
    free(foreign_key_id);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_foreign_cols_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN_COLS columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_innodb_foreign_cols_foreign_key_rows(
            database,
            rows,
            schema,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key
) {
    char *foreign_key_id = NULL;
    int rc =
        copy_information_schema_innodb_foreign_id(database, schema, foreign_key, &foreign_key_id);

    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key->part_count; ++index) {
        rc = append_information_schema_innodb_foreign_cols_part_row(
            database,
            rows,
            foreign_key_id,
            &foreign_key->parts[index]
        );
    }

    free(foreign_key_id);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *foreign_key_id,
    const struct loaded_foreign_key_part *part
) {
    char position_text[integer_text_capacity];
    int rc = information_schema_format_i64(
        database,
        part->foreign_key_column.ordinal_position,
        position_text,
        sizeof(position_text)
    );

    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_foreign_cols_column_count] = {
            foreign_key_id,
            part->child_column.name,
            part->parent_column.name,
            position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int copy_information_schema_innodb_foreign_id(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_id
) {
    return copy_information_schema_schema_object_name(
        database,
        schema->name,
        foreign_key->foreign_key.name,
        out_id
    );
}

static int copy_information_schema_innodb_foreign_table_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
) {
    return copy_information_schema_schema_object_name(
        database,
        schema->name,
        table->name,
        out_name
    );
}

static int copy_information_schema_schema_object_name(
    struct mylite_db *database,
    const char *schema_name,
    const char *object_name,
    char **out_name
) {
    size_t schema_length = strlen(schema_name);
    size_t object_length = strlen(object_name);
    size_t required_length = 0U;
    char *name = NULL;

    *out_name = NULL;
    if (schema_length > SIZE_MAX - object_length - tablespace_name_separator_size -
                            tablespace_name_terminator_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    required_length = schema_length + object_length + tablespace_name_separator_size +
                      tablespace_name_terminator_size;
    name = (char *)malloc(required_length);
    if (name == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(name, schema_name, schema_length);
    name[schema_length] = '/';
    memcpy(&name[schema_length + tablespace_name_separator_size], object_name, object_length);
    name[required_length - tablespace_name_terminator_size] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int information_schema_innodb_foreign_type(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    int64_t *out_type
) {
    static const struct information_schema_innodb_foreign_action_type_flags delete_flags = {
        .cascade = information_schema_innodb_foreign_delete_cascade_type,
        .set_null = information_schema_innodb_foreign_delete_set_null_type,
        .no_action = information_schema_innodb_foreign_delete_no_action_type,
    };
    static const struct information_schema_innodb_foreign_action_type_flags update_flags = {
        .cascade = information_schema_innodb_foreign_update_cascade_type,
        .set_null = information_schema_innodb_foreign_update_set_null_type,
        .no_action = information_schema_innodb_foreign_update_no_action_type,
    };
    int64_t delete_type = 0;
    int64_t update_type = 0;
    int rc = information_schema_innodb_foreign_action_type(
        database,
        foreign_key->delete_rule,
        &delete_flags,
        &delete_type
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_foreign_action_type(
            database,
            foreign_key->update_rule,
            &update_flags,
            &update_type
        );
    }
    if (rc == MYLITE_OK) {
        *out_type = delete_type | update_type;
    }
    return rc;
}

static int information_schema_innodb_foreign_action_type(
    struct mylite_db *database,
    const char *rule,
    const struct information_schema_innodb_foreign_action_type_flags *flags,
    int64_t *out_type
) {
    if (foreign_key_rule_is_cascade(rule)) {
        *out_type = flags->cascade;
        return MYLITE_OK;
    }
    if (foreign_key_rule_is_set_null(rule)) {
        *out_type = flags->set_null;
        return MYLITE_OK;
    }
    if (foreign_key_rule_equals(rule, "NO ACTION")) {
        *out_type = flags->no_action;
        return MYLITE_OK;
    }
    if (foreign_key_rule_equals(rule, "RESTRICT")) {
        *out_type = 0;
        return MYLITE_OK;
    }

    set_runtime_error(database, "unknown INFORMATION_SCHEMA.INNODB_FOREIGN action");
    return MYLITE_ERROR;
}

static int append_information_schema_table_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    size_t check_constraint_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

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
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_check_constraint_infos(
            database,
            table->table_id,
            &check_constraints,
            &check_constraint_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_table_constraints_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_table_constraints_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        rc = append_information_schema_table_constraints_check_row(
            database,
            rows,
            schema,
            table,
            &check_constraints[index]
        );
    }

    mylite_execution_loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_table_constraints_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    const char *constraint_type =
        index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ? "PRIMARY KEY" : "UNIQUE";
    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        index->index.name,
        schema->name,
        table->name,
        constraint_type,
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        schema->name,
        table->name,
        "FOREIGN KEY",
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_check_constraint_info *check_constraint
) {
    const char *enforced = "NO";

    if (check_constraint->check_constraint.is_enforced) {
        enforced = "YES";
    }

    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        check_constraint->check_constraint.name,
        schema->name,
        table->name,
        "CHECK",
        enforced,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_table_constraints_mysql_system_table_row(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_table_constraints_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        const char *values[information_schema_table_constraints_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            definition->schema_name,
            definition->query_definition.name,
            "PRIMARY KEY",
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];

        if (!secondary_index->is_unique) {
            continue;
        }
        const char *values[information_schema_table_constraints_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            definition->schema_name,
            definition->query_definition.name,
            "UNIQUE",
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_table_constraints_extensions_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

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
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_table_constraints_extensions_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_table_constraints_extensions_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_table_constraints_extensions_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    const char *values[information_schema_table_constraints_extensions_column_count] = {
        "def",
        schema->name,
        index->index.name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_extensions_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_table_constraints_extensions_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_extensions_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_table_constraints_extensions_mysql_system_table_row(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_table_constraints_extensions_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        const char *values[information_schema_table_constraints_extensions_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            definition->query_definition.name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        const char *values[information_schema_table_constraints_extensions_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            definition->query_definition.name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_check_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t check_constraint_count = 0U;
    int rc = mylite_execution_load_table_check_constraint_infos(
        database,
        table->table_id,
        &check_constraints,
        &check_constraint_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        rc = append_information_schema_check_constraints_check_row(
            database,
            rows,
            schema,
            &check_constraints[index]
        );
    }

    mylite_execution_loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    return rc;
}

static int append_information_schema_check_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_check_constraint_info *check_constraint
) {
    const char *values[information_schema_check_constraints_column_count] = {
        "def",
        schema->name,
        check_constraint->check_constraint.name,
        check_constraint->check_constraint.check_clause,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_key_column_usage_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

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
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_key_column_usage_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_key_column_usage_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_key_column_usage_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    char ordinal_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            schema->name,
            index->index.name,
            "def",
            schema->name,
            table->name,
            part->column.name,
            ordinal_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_key_column_usage_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    char ordinal_text[integer_text_capacity];
    char referenced_ordinal_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *part = &foreign_key->parts[part_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            schema->name,
            foreign_key->foreign_key.name,
            "def",
            schema->name,
            table->name,
            part->child_column.name,
            ordinal_text,
            referenced_ordinal_text,
            schema->name,
            foreign_key->parent_table.name,
            part->parent_column.name,
        };

        rc = information_schema_format_i64(
            database,
            part->foreign_key_column.ordinal_position,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                part->foreign_key_column.position_in_unique_constraint,
                referenced_ordinal_text,
                sizeof(referenced_ordinal_text)
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_key_column_usage_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_key_column_usage_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_key_column_usage_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            sequence_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (!secondary_index->is_unique) {
            continue;
        }
        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table unique key");
            return MYLITE_ERROR;
        }

        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[secondary_index->column_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            sequence_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(database, 1, sequence_text, sizeof(sequence_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_referential_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_referential_constraints_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_referential_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_referential_constraints_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        "def",
        schema->name,
        foreign_key->parent_index.name,
        foreign_key->foreign_key.match_option,
        foreign_key->foreign_key.update_rule,
        foreign_key->foreign_key.delete_rule,
        table->name,
        foreign_key->parent_table.name,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_statistics_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_statistics_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_statistics_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        const char *values[information_schema_statistics_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            "0",
            definition->schema_name,
            "PRIMARY",
            sequence_text,
            column->name,
            mysql_system_table_primary_key_collation(definition, key_index + 1),
            mysql_system_table_primary_key_cardinality(definition, key_index + 1),
            NULL,
            NULL,
            "",
            "BTREE",
            "",
            "",
            "YES",
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table secondary index");
            return MYLITE_ERROR;
        }

        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[secondary_index->column_index];
        const char *values[information_schema_statistics_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            secondary_index->non_unique,
            definition->schema_name,
            secondary_index->name,
            sequence_text,
            column->name,
            "A",
            secondary_index->cardinality,
            NULL,
            NULL,
            "",
            "BTREE",
            "",
            "",
            "YES",
            NULL,
        };

        rc = information_schema_format_i64(database, 1, sequence_text, sizeof(sequence_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_statistics_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

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
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_statistics_base_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_statistics_base_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    static const char cardinality[] = "0";
    char sequence[integer_text_capacity];
    const char *non_unique = "1";
    int rc = MYLITE_OK;

    if (index->index.is_unique) {
        non_unique = "0";
    }

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        char prefix_text[integer_text_capacity];
        const char *nullable = "";
        const bool is_fulltext = index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT;
        const bool is_spatial = index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL;
        const char *collation =
            part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC ? "D"
                                                                                          : "A";
        const char *prefix_value = NULL;
        const char *index_type = "BTREE";

        if (part->column.is_nullable) {
            nullable = "YES";
        }
        if (is_fulltext) {
            collation = NULL;
            index_type = "FULLTEXT";
        }
        if (is_spatial) {
            collation = "A";
            prefix_value = "32";
            index_type = "SPATIAL";
        }
        if (!is_fulltext && !is_spatial && part->index_column.has_prefix_length) {
            rc = information_schema_format_i64(
                database,
                part->index_column.prefix_length,
                prefix_text,
                sizeof(prefix_text)
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            prefix_value = prefix_text;
        }
        const char *values[information_schema_statistics_column_count] = {
            "def",
            schema->name,
            table->name,
            non_unique,
            schema->name,
            index->index.name,
            sequence,
            part->column.name,
            collation,
            cardinality,
            prefix_value,
            NULL,
            nullable,
            index_type,
            "",
            index->index.comment,
            index_visibility_text(index->index.is_visible),
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position,
            sequence,
            sizeof(sequence)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}
