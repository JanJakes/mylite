
static int validate_insert_select_json_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column
) {
    const unsigned char *text = NULL;
    int byte_count = 0;
    char *normalized = NULL;
    size_t normalized_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit JSON conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    rc = mylite_json_normalize(
        (const char *)text,
        (size_t)byte_count,
        &normalized,
        &normalized_length,
        &result
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        if (result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            set_unsupported_error(database, "INSERT ... SELECT does not support this JSON value");
        } else {
            set_invalid_json_text_error(database, result.position, target_column->name);
        }
        return MYLITE_ERROR;
    }
    free(normalized);
    (void)normalized_length;
    return MYLITE_OK;
}

static int validate_insert_select_binary_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const void *bytes = NULL;
    int byte_count = 0;
    char *copy = NULL;
    size_t copy_length = 0U;
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_BLOB) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit binary string conversion"
        );
        return MYLITE_ERROR;
    }
    bytes = sqlite3_column_blob(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    copy_length = (size_t)byte_count;
    copy = malloc(copy_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (copy_length != 0U) {
        memcpy(copy, bytes, copy_length);
    }
    copy[copy_length] = '\0';
    rc = convert_binary_bytes_for_column(
        database,
        target_column,
        row_number,
        false,
        &copy,
        &copy_length
    );
    free(copy);
    return rc;
}

static int validate_insert_select_bit_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const void *bytes = NULL;
    int byte_count = 0;
    uint64_t width = 0U;
    size_t expected_byte_count = 0U;
    unsigned char unused_mask = 0U;
    int rc = bit_width_for_logical_type(target_column->logical_type, &width);

    if (rc != MYLITE_OK || sqlite3_column_type(statement, selected_column_index) != SQLITE_BLOB) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit BIT conversion"
        );
        return MYLITE_ERROR;
    }
    bytes = sqlite3_column_blob(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    expected_byte_count = bit_byte_width(width);
    if ((size_t)byte_count != expected_byte_count) {
        set_data_too_long_error(database, target_column->name, row_number);
        return MYLITE_ERROR;
    }
    if ((width % byte_bit_count) != 0U && expected_byte_count > 0U) {
        unused_mask = (unsigned char)(byte_all_bits_mask << (width % byte_bit_count));
        if ((((const unsigned char *)bytes)[0] & unused_mask) != 0U) {
            set_data_too_long_error(database, target_column->name, row_number);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int validate_insert_select_decimal_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit DECIMAL conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    return validate_canonical_decimal_text(
        database,
        target_column,
        (const char *)text,
        (size_t)byte_count,
        row_number
    );
}

static int validate_insert_select_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int sqlite_type = sqlite3_column_type(statement, selected_column_index);
    double value = 0.0;
    int rc = approximate_type_info_for_logical_type(target_column->logical_type, &info);

    if (rc != MYLITE_OK || (sqlite_type != SQLITE_FLOAT && sqlite_type != SQLITE_INTEGER)) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit FLOAT or DOUBLE conversion"
        );
        return MYLITE_ERROR;
    }
    value = sqlite3_column_double(statement, selected_column_index);
    if (!isfinite(value) || (info.is_unsigned && value < 0.0) ||
        (info.type_class == APPROXIMATE_TYPE_FLOAT && fabs(value) > FLT_MAX)) {
        set_out_of_range_error(database, target_column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_date_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit DATE conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!predicate_date_text_admitted(database, (const char *)text, (size_t)byte_count)) {
        set_incorrect_date_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_time_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit TIME conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!time_text_is_canonical_valid((const char *)text, (size_t)byte_count)) {
        set_incorrect_time_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_datetime_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit DATETIME conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!predicate_datetime_text_admitted(database, (const char *)text, (size_t)byte_count)) {
        set_incorrect_datetime_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_timestamp_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit TIMESTAMP conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!predicate_timestamp_text_admitted(database, (const char *)text, (size_t)byte_count)) {
        set_incorrect_datetime_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_year_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit YEAR conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!year_text_is_canonical((const char *)text, (size_t)byte_count)) {
        set_out_of_range_error(database, target_column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value
) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int64_t selected_value = 0;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t converted_value = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_INTEGER) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit integer conversion"
        );
        return MYLITE_ERROR;
    }
    if (adjust_value) {
        return integer_range_for_column(
            database,
            target_column,
            "INSERT supports only baseline integer columns",
            &range
        );
    }

    selected_value = (int64_t)sqlite3_column_int64(statement, selected_column_index);
    is_negative = selected_value < 0;
    if (is_negative) {
        magnitude =
            selected_value == INT64_MIN ? int64_negative_abs_max : (uint64_t)(-selected_value);
    } else {
        magnitude = (uint64_t)selected_value;
    }

    return convert_integer_for_column(
        database,
        magnitude,
        is_negative,
        target_column,
        row_number,
        &converted_value
    );
}
