static bool column_descriptor_is_auto_increment(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column != NULL && column->is_auto_increment) != 0;
}

static int validate_planned_table_row_size(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
) {
    uint64_t total = 0U;
    int rc = MYLITE_OK;

    if (columns == NULL && column_count != 0U) {
        set_runtime_error(database, "invalid planned columns");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = add_planned_column_row_size(database, &columns[column_index], &total);
    }

    return rc;
}

static int validate_added_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct planned_column *added_column
) {
    uint64_t total = 0U;
    int rc = MYLITE_OK;

    if ((columns == NULL && column_count != 0U) || added_column == NULL) {
        set_runtime_error(database, "invalid add-column row-size plan");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = add_catalog_column_row_size(database, &columns[column_index], &total);
    }
    if (rc == MYLITE_OK) {
        rc = add_planned_column_row_size(database, added_column, &total);
    }

    return rc;
}

static int add_planned_column_row_size(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *total
) {
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || total == NULL) {
        set_runtime_error(database, "invalid planned column row-size input");
        return MYLITE_ERROR;
    }
    rc = row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "row-size validation supports only current baseline column descriptors",
        &size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return add_row_size_bytes(database, size, total);
}

static int add_catalog_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *total
) {
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || total == NULL) {
        set_runtime_error(database, "invalid catalog column row-size input");
        return MYLITE_ERROR;
    }
    rc = row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "row-size validation supports only current baseline column descriptors",
        &size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return add_row_size_bytes(database, size, total);
}

static int row_size_for_column_descriptor(
    struct mylite_db *database,
    const char *logical_type,
    const char *physical_type,
    const char *unsupported_message,
    uint64_t *out_size
) {
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (logical_type == NULL || physical_type == NULL || unsupported_message == NULL ||
        out_size == NULL) {
        set_runtime_error(database, "invalid row-size descriptor");
        return MYLITE_ERROR;
    }
    if (strcmp(physical_type, "INTEGER") == 0) {
        rc = integer_row_size_bytes(logical_type, &size);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        *out_size = size;
        return MYLITE_OK;
    }
    if (strcmp(physical_type, "REAL") == 0) {
        rc = approximate_type_info_for_logical_type(logical_type, &approximate_info);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        return approximate_row_size_bytes(&approximate_info, out_size);
    }
    if (strcmp(physical_type, "BLOB") == 0) {
        uint64_t bit_width = 0U;
        const char *spatial_type = NULL;

        if (spatial_logical_type_display_text(logical_type, &spatial_type)) {
            *out_size = blob_family_row_size_contribution;
            return MYLITE_OK;
        }
        if (bit_width_for_logical_type(logical_type, &bit_width) == MYLITE_OK) {
            *out_size = (uint64_t)bit_byte_width(bit_width);
            return MYLITE_OK;
        }
        return binary_string_row_size_bytes(database, logical_type, out_size, unsupported_message);
    }
    if (strcmp(physical_type, "TEXT") != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    return text_backed_row_size_bytes(database, logical_type, out_size, unsupported_message);
}

static int binary_string_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
) {
    struct binary_string_type_info info = {0};
    const struct binary_string_type_info *resolved_info =
        binary_string_type_info_for_logical_type(logical_type, &info);

    if (resolved_info == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (resolved_info->fixed_length) {
        *out_size = resolved_info->maximum_length;
        return MYLITE_OK;
    }
    if (!resolved_info->blob_family) {
        *out_size = resolved_info->maximum_length;
        if (resolved_info->maximum_length <= varchar_one_byte_length_prefix_max) {
            *out_size += 1U;
        } else {
            *out_size += 2U;
        }
        return MYLITE_OK;
    }
    *out_size = blob_family_row_size_contribution;
    return MYLITE_OK;
}

static int text_backed_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
) {
    struct decimal_type_info decimal_info = {0};
    size_t length = 0U;
    uint64_t size = 0U;
    int rc = MYLITE_OK;

    if (logical_type_is_varchar(logical_type)) {
        rc = parse_varchar_descriptor_length(database, logical_type, unsupported_message, &length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        size = (uint64_t)length * logical_type_max_bytes_per_character(logical_type);
        *out_size = size;
        if (size <= varchar_one_byte_length_prefix_max) {
            *out_size += 1U;
        } else {
            *out_size += 2U;
        }
        return MYLITE_OK;
    }
    if (logical_type_is_char(logical_type)) {
        rc = parse_char_descriptor_length(database, logical_type, unsupported_message, &length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_size = (uint64_t)length * logical_type_max_bytes_per_character(logical_type);
        return MYLITE_OK;
    }
    if (decimal_type_info_for_logical_type(logical_type, &decimal_info) == MYLITE_OK) {
        return decimal_row_size_bytes(&decimal_info, out_size);
    }
    if (strncmp(logical_type, "ENUM(", enum_logical_prefix_length) == 0) {
        struct enum_type_info enum_info = {0};

        rc = enum_type_info_for_logical_type(
            database,
            logical_type,
            unsupported_message,
            &enum_info
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_size = enum_info.label_count <= enum_one_byte_label_count_max ? 1U : 2U;
        return MYLITE_OK;
    }
    if (strncmp(logical_type, "SET(", set_logical_prefix_length) == 0) {
        return set_row_size_bytes(database, logical_type, out_size, unsupported_message);
    }
    if (strcmp(logical_type, "YEAR") == 0) {
        *out_size = year_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DATE") == 0) {
        *out_size = date_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TIME") == 0) {
        *out_size = time_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DATETIME") == 0) {
        *out_size = datetime_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TIMESTAMP") == 0) {
        *out_size = timestamp_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "JSON") == 0) {
        *out_size = text_family_row_size_contribution;
        return MYLITE_OK;
    }
    if (text_family_type_info_for_logical_type(logical_type) != NULL) {
        *out_size = text_family_row_size_contribution;
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int set_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
) {
    struct set_type_info info = {0};
    int rc = set_type_info_for_logical_type(database, logical_type, unsupported_message, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info.member_count <= set_one_byte_storage_member_count_max) {
        *out_size = set_one_byte_storage_size;
    } else if (info.member_count <= set_two_byte_storage_member_count_max) {
        *out_size = set_two_byte_storage_size;
    } else if (info.member_count <= set_three_byte_storage_member_count_max) {
        *out_size = set_three_byte_storage_size;
    } else if (info.member_count <= set_four_byte_storage_member_count_max) {
        *out_size = set_four_byte_storage_size;
    } else {
        *out_size = set_eight_byte_storage_size;
    }
    return MYLITE_OK;
}

static int integer_row_size_bytes(const char *logical_type, uint64_t *out_size) {
    if (logical_type == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_size = tinyint_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_size = smallint_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_size = mediumint_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INT UNSIGNED") == 0) {
        *out_size = int_row_size_bytes;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_size = bigint_row_size_bytes;
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static int decimal_row_size_bytes(const struct decimal_type_info *info, uint64_t *out_size) {
    uint64_t integer_digits = 0U;
    uint64_t fractional_digits = 0U;

    if (info == NULL || out_size == NULL || info->scale > info->precision) {
        return MYLITE_MISUSE;
    }

    integer_digits = info->precision - info->scale;
    fractional_digits = info->scale;
    *out_size =
        decimal_digit_group_size(integer_digits) + decimal_digit_group_size(fractional_digits);
    return MYLITE_OK;
}

static int approximate_row_size_bytes(
    const struct approximate_type_info *info,
    uint64_t *out_size
) {
    if (info == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        *out_size = float_row_size_bytes;
        return MYLITE_OK;
    }
    if (info->type_class == APPROXIMATE_TYPE_DOUBLE) {
        *out_size = double_row_size_bytes;
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static uint64_t decimal_digit_group_size(uint64_t digits) {
    static const uint64_t leftover_digit_bytes[] = {0U, 1U, 1U, 2U, 2U, 3U, 3U, 4U, 4U};
    uint64_t full_groups = digits / decimal_digits_per_storage_group;
    uint64_t leftover_digits = digits % decimal_digits_per_storage_group;

    return (full_groups * 4U) + leftover_digit_bytes[leftover_digits];
}

static int add_row_size_bytes(struct mylite_db *database, uint64_t addition, uint64_t *total) {
    if (total == NULL) {
        set_runtime_error(database, "invalid row-size accumulator");
        return MYLITE_ERROR;
    }
    if (*total > UINT64_MAX - addition) {
        set_row_size_too_large_error(database);
        return MYLITE_ERROR;
    }
    *total += addition;
    if (*total > mysql_max_row_size) {
        set_row_size_too_large_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
