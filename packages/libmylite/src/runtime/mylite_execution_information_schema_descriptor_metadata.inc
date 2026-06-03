
static int information_schema_character_metadata_for_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    char *character_length_text,
    size_t character_length_text_size,
    char *character_octet_text,
    size_t character_octet_text_size,
    struct information_schema_character_metadata *out_metadata
) {
    uint64_t character_length = 0U;
    uint64_t character_octet_length = 0U;
    bool has_character_metadata = false;
    bool has_binary_metadata = false;
    int rc = MYLITE_OK;

    *out_metadata = (struct information_schema_character_metadata){0};
    if (column_descriptor_is_char(column)) {
        size_t length = 0U;

        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline CHAR descriptors",
            &length
        );
        character_length = (uint64_t)length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_varchar(column)) {
        size_t length = 0U;

        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline VARCHAR descriptors",
            &length
        );
        character_length = (uint64_t)length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_text_family(column)) {
        const struct text_family_type_info *info =
            text_family_type_info_for_logical_type(column->logical_type);

        if (info == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA.COLUMNS supports only baseline TEXT descriptors"
            );
            return MYLITE_ERROR;
        }
        character_length = info->maximum_length;
        character_octet_length = info->maximum_length;
        has_character_metadata = true;
    } else if (column_descriptor_is_enum(column)) {
        struct enum_type_info info = {0};

        rc = enum_type_info_for_logical_type(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline ENUM descriptors",
            &info
        );
        character_length = (uint64_t)info.max_label_character_length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_set(column)) {
        struct set_type_info info = {0};

        rc = set_type_info_for_logical_type(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline SET descriptors",
            &info
        );
        character_length = (uint64_t)info.max_display_character_length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_binary_string_family(column)) {
        struct binary_string_type_info storage = {0};
        const struct binary_string_type_info *info =
            binary_string_type_info_for_logical_type(column->logical_type, &storage);

        if (info == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA.COLUMNS supports only baseline binary descriptors"
            );
            return MYLITE_ERROR;
        }
        character_length = info->maximum_length;
        character_octet_length = info->maximum_length;
        has_character_metadata = true;
        has_binary_metadata = true;
    }
    if (rc != MYLITE_OK || !has_character_metadata) {
        return rc;
    }

    rc = information_schema_format_i64(
        database,
        (int64_t)character_length,
        character_length_text,
        character_length_text_size
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)character_octet_length,
            character_octet_text,
            character_octet_text_size
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *character_set_name = column_effective_character_set_name(table, column);
    const char *collation_name = column_effective_collation_name(table, column);

    *out_metadata = (struct information_schema_character_metadata){
        .character_maximum_length = character_length_text,
        .character_octet_length = character_octet_text,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
    };
    if (has_binary_metadata) {
        out_metadata->character_set_name = NULL;
        out_metadata->collation_name = NULL;
    }
    return MYLITE_OK;
}

static const char *information_schema_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = column->logical_type;
    const char *spatial_type = NULL;

    if (column_descriptor_is_char(column)) {
        return "char";
    }
    if (column_descriptor_is_varchar(column)) {
        return "varchar";
    }
    if (column_descriptor_is_text_family(column)) {
        return information_schema_text_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_enum(column)) {
        return "enum";
    }
    if (column_descriptor_is_set(column)) {
        return "set";
    }
    if (column_descriptor_is_json(column)) {
        return "json";
    }
    if (spatial_logical_type_display_text(logical_type, &spatial_type)) {
        return spatial_type;
    }
    if (column_descriptor_is_bit(column)) {
        return "bit";
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return information_schema_binary_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_decimal(column)) {
        return "decimal";
    }
    if (column_descriptor_is_approximate(column)) {
        return information_schema_approximate_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_date(column)) {
        return "date";
    }
    if (column_descriptor_is_time(column)) {
        return "time";
    }
    if (column_descriptor_is_datetime(column)) {
        return "datetime";
    }
    if (column_descriptor_is_timestamp(column)) {
        return "timestamp";
    }
    if (column_descriptor_is_year(column)) {
        return "year";
    }
    if (logical_type == NULL) {
        return "";
    }
    return information_schema_integer_data_type_for_descriptor(logical_type);
}

static const char *information_schema_text_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const struct text_family_type_info *info =
        text_family_type_info_for_logical_type(column->logical_type);

    return info == NULL ? "" : info->display_type;
}

static const char *information_schema_binary_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(column->logical_type, &storage);
    const char *logical_type = column->logical_type;

    if (info == NULL || logical_type == NULL) {
        return "";
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        return "binary";
    }
    if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        return "varbinary";
    }
    if (info->display_type == NULL) {
        return "";
    }
    return info->display_type;
}

static const char *information_schema_approximate_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK &&
        info.type_class == APPROXIMATE_TYPE_FLOAT) {
        return "float";
    }
    return "double";
}

static const char *information_schema_integer_data_type_for_descriptor(const char *logical_type) {
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        return "tinyint";
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        return "smallint";
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        return "mediumint";
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        return "bigint";
    }
    return "int";
}

static const char *information_schema_numeric_precision_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = column->logical_type;

    if (column_descriptor_is_bit(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_spatial(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return NULL;
    }
    if (column_descriptor_is_approximate(column)) {
        struct approximate_type_info info = {
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };

        if (approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK &&
            info.type_class == APPROXIMATE_TYPE_FLOAT) {
            return "12";
        }
        return "22";
    }
    if (logical_type == NULL) {
        return "10";
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        return "3";
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        return "5";
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        return "7";
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        return "20";
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        return "19";
    }
    return "10";
}

static const char *information_schema_numeric_scale_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_bit(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_spatial(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_approximate(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return NULL;
    }
    return "0";
}
