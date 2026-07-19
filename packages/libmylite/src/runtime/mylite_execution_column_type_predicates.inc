static const char *logical_type_for_mapped_integer(struct mapped_integer_type integer_type) {
    switch (integer_type.type) {
    case MYLITE_SQL_AST_INTEGER_TYPE_NONE:
        return NULL;
    case MYLITE_SQL_AST_INTEGER_TYPE_TINYINT:
        if (integer_type.is_unsigned != 0) {
            return "TINYINT UNSIGNED";
        }
        if (integer_type.is_bool_alias) {
            return "TINYINT(1)";
        }
        if (integer_type.has_display_width && integer_type.display_width == 1U) {
            return "TINYINT(1)";
        }
        return "TINYINT";
    case MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT:
        return integer_type.is_unsigned == 0 ? "SMALLINT" : "SMALLINT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT:
        return integer_type.is_unsigned == 0 ? "MEDIUMINT" : "MEDIUMINT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_INT:
        return integer_type.is_unsigned == 0 ? "INT" : "INT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_BIGINT:
        return integer_type.is_unsigned == 0 ? "BIGINT" : "BIGINT UNSIGNED";
    }

    return NULL;
}

static bool logical_type_is_varchar(const char *logical_type) {
    return (logical_type != NULL &&
            (strncmp(logical_type, "VARCHAR(", varchar_logical_prefix_length) == 0 ||
             strncmp(logical_type, "NVARCHAR(", nvarchar_logical_prefix_length) == 0)) != 0;
}

static bool logical_type_is_char(const char *logical_type) {
    return (logical_type != NULL &&
            (strncmp(logical_type, "CHAR(", char_logical_prefix_length) == 0 ||
             strncmp(logical_type, "NCHAR(", nchar_logical_prefix_length) == 0)) != 0;
}

static bool logical_type_is_national_varchar(const char *logical_type) {
    return (logical_type != NULL &&
            strncmp(logical_type, "NVARCHAR(", nvarchar_logical_prefix_length) == 0) != 0;
}

static bool logical_type_is_national_char(const char *logical_type) {
    return (logical_type != NULL &&
            strncmp(logical_type, "NCHAR(", nchar_logical_prefix_length) == 0) != 0;
}

static bool logical_type_is_national_char_or_varchar(const char *logical_type) {
    return (logical_type_is_national_char(logical_type) ||
            logical_type_is_national_varchar(logical_type)) != 0;
}

static size_t char_descriptor_prefix_length(const char *logical_type) {
    if (logical_type_is_national_char(logical_type)) {
        return nchar_logical_prefix_length;
    }
    if (logical_type != NULL && strncmp(logical_type, "CHAR(", char_logical_prefix_length) == 0) {
        return char_logical_prefix_length;
    }
    return 0U;
}

static size_t char_descriptor_syntax_overhead(const char *logical_type) {
    if (logical_type_is_national_char(logical_type)) {
        return nchar_logical_syntax_overhead;
    }
    return char_logical_syntax_overhead;
}

static size_t varchar_descriptor_prefix_length(const char *logical_type) {
    if (logical_type_is_national_varchar(logical_type)) {
        return nvarchar_logical_prefix_length;
    }
    if (logical_type != NULL &&
        strncmp(logical_type, "VARCHAR(", varchar_logical_prefix_length) == 0) {
        return varchar_logical_prefix_length;
    }
    return 0U;
}

static size_t varchar_descriptor_syntax_overhead(const char *logical_type) {
    if (logical_type_is_national_varchar(logical_type)) {
        return nvarchar_logical_syntax_overhead;
    }
    return varchar_logical_syntax_overhead;
}

static uint64_t logical_type_max_bytes_per_character(const char *logical_type) {
    if (logical_type_is_national_char_or_varchar(logical_type)) {
        return utf8mb3_max_bytes_per_character;
    }
    return utf8mb4_max_bytes_per_character;
}

static bool planned_column_is_varchar(const struct planned_column *column) {
    bool has_varchar_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    if (logical_type_is_varchar(column->logical_type)) {
        has_varchar_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_varchar_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool planned_column_is_char(const struct planned_column *column) {
    bool has_char_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    if (logical_type_is_char(column->logical_type)) {
        has_char_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_char_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool planned_column_is_char_or_varchar(const struct planned_column *column) {
    return (planned_column_is_char(column) || planned_column_is_varchar(column)) != 0;
}

static bool planned_column_is_text_family(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return text_family_type_info_for_logical_type(column->logical_type) != NULL;
}

static bool planned_column_is_string_family(const struct planned_column *column) {
    return (planned_column_is_char(column) || planned_column_is_varchar(column) ||
            planned_column_is_text_family(column)) != 0;
}

static bool planned_column_is_json(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "JSON") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool planned_column_is_spatial(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_spatial(&descriptor);
}

static bool planned_column_is_enum(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strncmp(column->logical_type, "ENUM(", enum_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_set(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strncmp(column->logical_type, "SET(", set_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_binary_string_family(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_binary_string_family(&descriptor);
}

static bool planned_column_is_binary_blob_family(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_binary_blob_family(&descriptor);
}

static bool planned_column_is_bit(const struct planned_column *column) {
    struct mylite_catalog_column_descriptor descriptor = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }

    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    return column_descriptor_is_bit(&descriptor);
}

static bool planned_column_is_year(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "YEAR") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool planned_column_is_decimal(const struct planned_column *column) {
    struct decimal_type_info info = {0};

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return decimal_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool planned_column_is_approximate(const struct planned_column *column) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "REAL") != 0) {
        return false;
    }
    return approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool planned_column_is_date(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "DATE") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool planned_column_is_time(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "TIME") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool planned_column_is_datetime(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "DATETIME") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool planned_column_is_timestamp(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    return (strcmp(column->logical_type, "TIMESTAMP") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_default_kind_has_integer_value(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) != 0;
}

static bool column_default_kind_has_text_value(enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_SCALAR_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) != 0;
}

static bool column_default_kind_is_expression(enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_SCALAR_EXPRESSION) != 0;
}

static bool column_default_kind_materializes_value(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_SCALAR_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) != 0;
}

static bool column_descriptor_physical_type_equals(
    const struct mylite_catalog_column_descriptor *column,
    const char *physical_type
) {
    size_t physical_type_length = 0U;

    if (column == NULL || column->physical_type[0] == '\0' || physical_type == NULL) {
        return false;
    }

    physical_type_length = strlen(physical_type);
    if (physical_type_length >= sizeof(column->physical_type)) {
        return false;
    }

    return (memcmp(column->physical_type, physical_type, physical_type_length) == 0 &&
            column->physical_type[physical_type_length] == '\0') != 0;
}

static bool column_descriptor_is_varchar(const struct mylite_catalog_column_descriptor *column) {
    bool has_varchar_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }

    if (logical_type_is_varchar(column->logical_type)) {
        has_varchar_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_varchar_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool column_descriptor_is_char(const struct mylite_catalog_column_descriptor *column) {
    bool has_char_logical_type = false;
    bool has_text_physical_type = false;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }

    if (logical_type_is_char(column->logical_type)) {
        has_char_logical_type = true;
    }
    if (strcmp(column->physical_type, "TEXT") == 0) {
        has_text_physical_type = true;
    }
    if (!has_char_logical_type) {
        return false;
    }
    if (!has_text_physical_type) {
        return false;
    }
    return true;
}

static bool column_descriptor_is_national_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return logical_type_is_national_char_or_varchar(column->logical_type);
}

static bool column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_descriptor_is_char(column) || column_descriptor_is_varchar(column)) != 0;
}

static bool column_descriptor_is_text_family(const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return text_family_type_info_for_logical_type(column->logical_type) != NULL;
}

static bool column_descriptor_is_string_family(const struct mylite_catalog_column_descriptor *column
) {
    return (column_descriptor_is_char(column) || column_descriptor_is_varchar(column) ||
            column_descriptor_is_text_family(column)) != 0;
}

static bool column_descriptor_is_json(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "JSON") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool column_descriptor_is_spatial(const struct mylite_catalog_column_descriptor *column) {
    static const char *const spatial_types[] = {
        "geometry",
        "point",
        "linestring",
        "polygon",
        "multipoint",
        "multilinestring",
        "multipolygon",
        "geomcollection",
    };

    if (column == NULL || strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(spatial_types) / sizeof(spatial_types[0]); ++index) {
        if (strcmp(column->logical_type, spatial_types[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool column_descriptor_is_enum(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strncmp(column->logical_type, "ENUM(", enum_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_uses_enum_implicit_missing_default(
    const struct mylite_catalog_column_descriptor *column
) {
    if (!column_descriptor_is_enum(column) || column->is_nullable) {
        return false;
    }
    return (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT) != 0;
}

static bool column_descriptor_is_set(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strncmp(column->logical_type, "SET(", set_logical_prefix_length) == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

bool mylite_execution_column_descriptor_is_binary_value(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column);
}

static bool column_descriptor_is_binary_string_family(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    return binary_string_type_info_for_logical_type(column->logical_type, &storage) != NULL;
}

static bool column_descriptor_is_binary_blob_family(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    info = binary_string_type_info_for_logical_type(column->logical_type, &storage);
    return (info != NULL && info->blob_family) != 0;
}

static bool column_descriptor_is_bit(const struct mylite_catalog_column_descriptor *column) {
    uint64_t width = 0U;

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "BLOB") != 0) {
        return false;
    }
    return bit_width_for_logical_type(column->logical_type, &width) == MYLITE_OK;
}

static bool column_descriptor_is_year(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "YEAR") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool column_descriptor_is_decimal(const struct mylite_catalog_column_descriptor *column) {
    struct decimal_type_info info = {0};

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return decimal_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool column_descriptor_is_approximate(const struct mylite_catalog_column_descriptor *column
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    if (strcmp(column->physical_type, "REAL") != 0) {
        return false;
    }
    return approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK;
}

static bool column_descriptor_is_date(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "DATE") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool column_descriptor_is_time(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "TIME") == 0 && strcmp(column->physical_type, "TEXT") == 0
           ) != 0;
}

static bool column_descriptor_is_datetime(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "DATETIME") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}

static bool column_descriptor_is_timestamp(const struct mylite_catalog_column_descriptor *column) {
    if (column == NULL || column->logical_type[0] == '\0' || column->physical_type[0] == '\0') {
        return false;
    }
    return (strcmp(column->logical_type, "TIMESTAMP") == 0 &&
            strcmp(column->physical_type, "TEXT") == 0) != 0;
}
