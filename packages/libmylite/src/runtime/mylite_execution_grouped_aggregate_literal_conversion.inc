static int convert_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    rc = decode_dml_character_literal(
        database,
        value_node,
        "DATE values support only string, numeric, boolean, hex, NULL, and DEFAULT values",
        "DATE values do not support NUL bytes",
        false,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

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

static int convert_relaxed_date_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[date_text_length + 1U];
    char *copy = NULL;
    struct temporal_predicate_normalization_input normalization = {0};
    enum temporal_storage_truncation truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
    int rc = MYLITE_OK;

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_date_storage_text(&normalization, normalized, &truncation)) {
        if (truncation == TEMPORAL_STORAGE_TRUNCATION_WARNING && !ignore_errors &&
            temporal_sql_mode_is_strict(database)) {
            set_incorrect_date_value_error(database, text, column_name, row_number);
            free(text);
            return MYLITE_ERROR;
        }
        copy = copy_temporal_text(database, normalized, date_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_date_text(
            database,
            copy,
            date_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        if (truncation == TEMPORAL_STORAGE_TRUNCATION_NOTE) {
            if (temporal_sql_mode_is_strict(database)) {
                rc = append_incorrect_date_value_note(database, text, column_name, row_number);
            } else {
                rc = append_data_truncated_note(database, column_name, row_number);
            }
        } else if (truncation == TEMPORAL_STORAGE_TRUNCATION_WARNING) {
            rc = append_data_truncated_warning(database, column_name, row_number);
        }
        free(text);
        return rc;
    }

    return canonicalize_date_text(
        database,
        text,
        text_length,
        column_name,
        row_number,
        ignore_errors,
        out_value
    );
}

static bool normalize_date_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text,
    enum temporal_storage_truncation *out_truncation
) {
    char normalized_datetime[datetime_text_length + 1U];
    const char *text = input == NULL ? NULL : input->text;

    if (input == NULL || text == NULL || out_text == NULL || out_truncation == NULL) {
        return false;
    }

    if (input->text_length == datetime_text_length &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(normalized_datetime, text, datetime_text_length);
        normalized_datetime[datetime_date_time_separator_offset] = ' ';
        normalized_datetime[datetime_text_length] = '\0';
        if (!datetime_text_has_canonical_shape(normalized_datetime, datetime_text_length)) {
            return false;
        }
        memcpy(out_text, normalized_datetime, date_text_length);
        out_text[date_text_length] = '\0';
        if (datetime_storage_time_is_midnight(text)) {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
        } else {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NOTE;
        }
        return date_text_has_canonical_shape(out_text, date_text_length);
    }

    if (normalize_iso_temporal_predicate_text(input, normalized_datetime)) {
        memcpy(out_text, normalized_datetime, date_text_length);
        out_text[date_text_length] = '\0';
        if (datetime_storage_time_is_midnight(text)) {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
        } else {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NOTE;
        }
        return date_text_has_canonical_shape(out_text, date_text_length);
    }
    if (normalize_fractional_datetime_storage_text(input, normalized_datetime)) {
        memcpy(out_text, normalized_datetime, date_text_length);
        out_text[date_text_length] = '\0';
        if (datetime_storage_time_is_midnight(text)) {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
        } else {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NOTE;
        }
        return date_text_has_canonical_shape(out_text, date_text_length);
    }
    if (!normalize_z_temporal_predicate_text(text, input->text_length, normalized_datetime)) {
        return false;
    }

    memcpy(out_text, normalized_datetime, date_text_length);
    out_text[date_text_length] = '\0';
    *out_truncation = TEMPORAL_STORAGE_TRUNCATION_WARNING;
    return date_text_has_canonical_shape(out_text, date_text_length);
}

static bool normalize_fractional_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
) {
    enum { fractional_temporal_suffix_max_length = 7U };
    const char *text = input == NULL ? NULL : input->text;

    if (input == NULL || text == NULL || out_text == NULL ||
        input->text_length <= datetime_text_length + 1U ||
        input->text_length > datetime_text_length + fractional_temporal_suffix_max_length ||
        text[datetime_text_length] != '.') {
        return false;
    }
    memcpy(out_text, text, datetime_text_length);
    out_text[datetime_text_length] = '\0';
    if (!datetime_text_has_canonical_shape(out_text, datetime_text_length)) {
        return false;
    }
    for (size_t index = datetime_text_length + 1U; index < input->text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
    }

    return true;
}

static bool normalize_date_only_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
) {
    if (input == NULL || input->text == NULL || out_text == NULL ||
        input->text_length != date_text_length ||
        !date_text_has_canonical_shape(input->text, input->text_length)) {
        return false;
    }

    memcpy(out_text, input->text, date_text_length);
    memcpy(out_text + date_text_length, " 00:00:00", datetime_text_length - date_text_length);
    out_text[datetime_text_length] = '\0';
    return true;
}

static bool datetime_storage_time_is_midnight(const char *text) {
    if (text == NULL) {
        return false;
    }
    return (text[datetime_hour_text_offset] == '0' && text[datetime_hour_text_offset + 1U] == '0' &&
            text[datetime_minute_text_offset] == '0' &&
            text[datetime_minute_text_offset + 1U] == '0' &&
            text[datetime_second_text_offset] == '0' &&
            text[datetime_second_text_offset + 1U] == '0') != 0;
}

static int canonicalize_date_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    enum temporal_text_kind kind = TEMPORAL_TEXT_INVALID;
    bool strict = false;

    if (text == NULL || column_name == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }
    if (!date_text_has_canonical_shape(text, text_length)) {
        strict = temporal_sql_mode_is_strict(database);
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        set_incorrect_date_value_error(database, text, column_name, row_number);
        free(text);
        return MYLITE_ERROR;
    }

    kind = classify_date_text(text, text_length);
    strict = temporal_sql_mode_is_strict(database);
    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return assign_text_value(database, text, text_length, out_value);
    case TEMPORAL_TEXT_FULL_ZERO:
        if (!temporal_sql_mode_has_no_zero_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (!temporal_sql_mode_has_no_zero_in_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        if (temporal_sql_mode_has_allow_invalid_dates(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_INVALID:
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_DEFERRED:
        break;
    }

    set_incorrect_date_value_error(database, text, column_name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static int convert_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    rc = decode_dml_character_literal(
        database,
        value_node,
        "TIME values support only string, numeric, boolean, hex, NULL, and DEFAULT values",
        "TIME values do not support NUL bytes",
        false,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

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

static int canonicalize_time_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    bool is_negative = false;
    bool store_zero = false;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || column_name == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }
    {
        char normalized[time_text_maximum_length + 1U];
        size_t normalized_length = 0U;

        if (normalize_time_numeric_text(text, text_length, normalized, &normalized_length)) {
            char *copy = copy_temporal_text(database, normalized, normalized_length);

            free(text);
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            text = copy;
            text_length = normalized_length;
        }
    }
    {
        char normalized[time_text_maximum_length + 1U];
        size_t normalized_length = 0U;

        if (normalize_fractional_time_text(text, text_length, normalized, &normalized_length)) {
            char *copy = copy_temporal_text(database, normalized, normalized_length);

            free(text);
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            text = copy;
            text_length = normalized_length;
        }
    }
    if (time_text_is_canonical_valid(text, text_length)) {
        return assign_text_value(database, text, text_length, out_value);
    }
    if (!time_text_has_canonical_shape(text, text_length)) {
        if (ignore_errors) {
            rc = append_data_truncated_warning(database, column_name, row_number);
            free(text);
            if (rc != MYLITE_OK) {
                return rc;
            }
            return make_zero_time_value(database, out_value);
        }
        set_unsupported_error(database, "TIME values support only canonical [-]HH:MM:SS strings");
        free(text);
        return MYLITE_ERROR;
    }

    if (!ignore_errors) {
        set_incorrect_time_value_error(database, text, column_name, row_number);
        free(text);
        return MYLITE_ERROR;
    }

    if (!time_text_to_components(text, text_length, &is_negative, &hour, &minute, &second)) {
        free(text);
        return MYLITE_ERROR;
    }
    store_zero =
        (!time_text_uses_canonical_hour_width(text, text_length, &hour) ||
         minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second ||
         (hour == 0U && minute == 0U && second == 0U)) != 0;
    rc = append_out_of_range_warning(database, column_name, row_number);
    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (store_zero) {
        return make_zero_time_value(database, out_value);
    }
    if (is_negative) {
        return copy_text_value(database, "-838:59:59", out_value);
    }
    return copy_text_value(database, "838:59:59", out_value);
}

static bool normalize_time_numeric_text(
    const char *text,
    size_t text_length,
    char *out_text,
    size_t *out_text_length
) {
    enum {
        packed_time_max_digit_count = 6U,
        packed_time_hour_divisor = 10000U,
        packed_time_minute_divisor = 100U,
    };
    bool is_negative = false;
    size_t digit_offset = 0U;
    uint32_t packed = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int written = 0;

    if (text == NULL || out_text == NULL || out_text_length == NULL || text_length == 0U) {
        return false;
    }
    if (text[0] == '-') {
        is_negative = true;
        digit_offset = 1U;
    }
    if (digit_offset == text_length || text_length - digit_offset > packed_time_max_digit_count) {
        return false;
    }
    for (size_t index = digit_offset; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        packed = (packed * (uint32_t)decimal_base) + (uint32_t)(byte - '0');
    }

    hour = packed / packed_time_hour_divisor;
    minute = (packed / packed_time_minute_divisor) % packed_time_minute_divisor;
    second = packed % packed_time_minute_divisor;
    written = snprintf(
        out_text,
        time_text_maximum_length + 1U,
        "%s%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        is_negative && packed != 0U ? "-" : "",
        hour,
        minute,
        second
    );
    if (written < 0 || (size_t)written > time_text_maximum_length) {
        return false;
    }

    *out_text_length = (size_t)written;
    return true;
}

static bool normalize_fractional_time_text(
    const char *text,
    size_t text_length,
    char *out_text,
    size_t *out_text_length
) {
    enum {
        fractional_temporal_max_digits = 6U,
        fractional_temporal_suffix_max_length = 7U,
    };
    size_t prefix_length = 0U;

    if (text == NULL || out_text == NULL || out_text_length == NULL ||
        text_length <= time_text_minimum_length + 1U ||
        text_length > time_text_maximum_length + fractional_temporal_suffix_max_length) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == '.') {
            prefix_length = index;
            break;
        }
    }
    if (prefix_length == 0U || prefix_length > time_text_maximum_length ||
        !time_text_has_canonical_shape(text, prefix_length)) {
        return false;
    }
    if (text_length - prefix_length - 1U > fractional_temporal_max_digits) {
        return false;
    }
    for (size_t index = prefix_length + 1U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
    }
    memcpy(out_text, text, prefix_length);
    out_text[prefix_length] = '\0';
    *out_text_length = prefix_length;
    return true;
}

static int convert_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    rc = decode_dml_character_literal(
        database,
        value_node,
        "DATETIME values support only string, numeric, boolean, hex, NULL, and DEFAULT values",
        "DATETIME values do not support NUL bytes",
        false,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

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

static int convert_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    rc = decode_dml_character_literal(
        database,
        value_node,
        "TIMESTAMP values support only string, numeric, boolean, hex, NULL, and DEFAULT values",
        "TIMESTAMP values do not support NUL bytes",
        false,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

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

static int convert_relaxed_datetime_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = target_offset_minutes;
    if (normalize_date_only_datetime_storage_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_fractional_datetime_storage_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!ignore_errors && temporal_sql_mode_is_strict(database)) {
            set_incorrect_datetime_value_error(database, text, column_name, row_number);
            free(text);
            return MYLITE_ERROR;
        }
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column_name, row_number);
        }
        free(text);
        return rc;
    }

    return canonicalize_datetime_text(
        database,
        text,
        text_length,
        column_name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int canonicalize_timestamp_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (timestamp_text_is_canonical_valid(text, text_length)) {
        return assign_text_value(database, text, text_length, out_value);
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        if (ignore_errors || !temporal_sql_mode_is_strict(database)) {
            return make_timestamp_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        set_incorrect_datetime_value_error(database, text, column_name, row_number);
        free(text);
        return MYLITE_ERROR;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        if (!temporal_sql_mode_has_no_zero_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !temporal_sql_mode_is_strict(database)) {
            return make_timestamp_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
    } else if (ignore_errors || !temporal_sql_mode_is_strict(database)) {
        return make_timestamp_zero_with_warning(database, text, column_name, row_number, out_value);
    }

    set_incorrect_datetime_value_error(database, text, column_name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static int convert_relaxed_timestamp_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = target_offset_minutes;
    if (normalize_date_only_datetime_storage_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_fractional_datetime_storage_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!ignore_errors && temporal_sql_mode_is_strict(database)) {
            set_incorrect_datetime_value_error(database, text, column_name, row_number);
            free(text);
            return MYLITE_ERROR;
        }
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column_name, row_number);
        }
        free(text);
        return rc;
    }

    return canonicalize_timestamp_text(
        database,
        text,
        text_length,
        column_name,
        row_number,
        ignore_errors,
        out_value
    );
}

static char *copy_temporal_text(struct mylite_db *database, const char *text, size_t text_length) {
    char *copy = NULL;

    if (text == NULL) {
        return NULL;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return NULL;
    }
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';
    return copy;
}

static int convert_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(value_node);
    bool is_negative = false;
    bool parse_overflow = false;
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    uint64_t magnitude = 0U;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || column == NULL || out_value == NULL) {
        set_unsupported_error(
            database,
            "YEAR values support only integer, boolean, string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal(
            database,
            literal,
            "YEAR values support only string literals",
            "YEAR values do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return convert_year_string_value(
            database,
            text,
            text_length,
            column,
            row_number,
            adjust_errors,
            out_value
        );
    }

    rc = parse_year_value_magnitude(literal, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "YEAR values support only integer, boolean, string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (parse_overflow) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return make_zero_year_value_with_warning(
            database,
            column,
            row_number,
            false,
            NULL,
            out_value
        );
    }

    return convert_year_integer_value(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        adjust_errors,
        out_value
    );
}

static int convert_year_integer_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint32_t year = 0U;

    if (!convert_year_numeric_magnitude(magnitude, is_negative, &year)) {
        if (ignore_errors) {
            return make_zero_year_value_with_warning(
                database,
                column,
                row_number,
                false,
                NULL,
                out_value
            );
        }
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return make_year_value(database, year, out_value);
}

static int convert_year_string_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint32_t year = 0U;
    int rc = convert_year_string_magnitude(text, text_length, &year);

    if (rc == MYLITE_OK) {
        free(text);
        return make_year_value(database, year, out_value);
    }
    if (ignore_errors) {
        bool incorrect_integer = rc == year_conversion_incorrect_integer;

        return make_zero_year_value_with_warning(
            database,
            column,
            row_number,
            incorrect_integer,
            text,
            out_value
        );
    }
    if (rc == year_conversion_incorrect_integer) {
        set_incorrect_integer_value_error(database, text, column->name, row_number);
        free(text);
        return MYLITE_ERROR;
    }

    set_out_of_range_error(database, column->name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static int convert_year_string_magnitude(const char *text, size_t text_length, uint32_t *out_year) {
    uint64_t magnitude = 0U;
    bool overflow = false;

    if (text == NULL || out_year == NULL || text_length == 0U) {
        return year_conversion_incorrect_integer;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            if (year_string_accepts_trailing_hyphen(
                    (struct year_string_trailing_hyphen_request){
                        .index = index,
                        .byte = byte,
                        .magnitude = magnitude,
                    }
                )) {
                *out_year = (uint32_t)magnitude;
                return MYLITE_OK;
            }
            return year_conversion_incorrect_integer;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            overflow = true;
        } else if (!overflow) {
            magnitude = (magnitude * decimal_base) + digit;
        }
    }
    if (overflow) {
        return MYLITE_ERROR;
    }
    return convert_year_string_magnitude_by_length(
        (struct year_string_magnitude_conversion){
            .magnitude = magnitude,
            .text_length = text_length,
        },
        out_year
    );
}

static bool year_string_accepts_trailing_hyphen(
    struct year_string_trailing_hyphen_request request
) {
    if (request.index != year_text_length || request.byte != '-') {
        return false;
    }
    return request.magnitude == 0U ||
           (request.magnitude >= year_minimum_normal && request.magnitude <= year_maximum);
}

static int convert_year_string_magnitude_by_length(
    struct year_string_magnitude_conversion conversion,
    uint32_t *out_year
) {
    if (conversion.text_length == 1U || conversion.text_length == 2U) {
        if (conversion.magnitude == 0U) {
            *out_year = (uint32_t)year_two_digit_high_base;
            return MYLITE_OK;
        }
        if (convert_year_numeric_magnitude(conversion.magnitude, false, out_year)) {
            return MYLITE_OK;
        }
        return MYLITE_ERROR;
    }
    if (conversion.text_length == year_text_length) {
        if (conversion.magnitude == 0U) {
            *out_year = 0U;
            return MYLITE_OK;
        }
        if (conversion.magnitude >= year_minimum_normal &&
            conversion.magnitude <= year_maximum) {
            *out_year = (uint32_t)conversion.magnitude;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int convert_year_direct_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(value_node);
    bool is_negative = false;
    bool parse_overflow = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL) {
        set_unsupported_error(database, "WHERE YEAR predicates support only scalar literals");
        return MYLITE_ERROR;
    }
    if (literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        uint32_t represented_year = 0U;

        rc = decode_sql_string_literal(
            database,
            literal,
            "WHERE YEAR predicates support only string literals",
            "WHERE YEAR predicate literals do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = convert_year_direct_string(text, text_length, &represented_year);
        free(text);
        if (rc != MYLITE_OK) {
            return make_year_value(database, 1U, out_value);
        }
        return make_year_value(database, represented_year, out_value);
    }

    rc = parse_year_value_magnitude(literal, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK || parse_overflow || is_negative) {
        return make_year_value(database, 1U, out_value);
    }
    if (magnitude == 0U) {
        return make_year_value(database, 0U, out_value);
    }
    if (magnitude <= year_direct_maximum) {
        return make_year_value(database, (uint32_t)magnitude, out_value);
    }
    return make_year_value(database, 1U, out_value);
}

static int convert_year_direct_string(const char *text, size_t text_length, uint32_t *out_year) {
    uint64_t magnitude = 0U;
    bool overflow = false;

    if (text == NULL || text_length == 0U || out_year == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            overflow = true;
        } else if (!overflow) {
            magnitude = (magnitude * decimal_base) + digit;
        }
    }
    if (overflow || magnitude > year_direct_maximum) {
        return MYLITE_ERROR;
    }

    *out_year = (uint32_t)magnitude;
    return MYLITE_OK;
}

static int parse_year_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node == NULL || out_magnitude == NULL || out_is_negative == NULL ||
        out_parse_overflow == NULL) {
        return MYLITE_MISUSE;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    *out_parse_overflow = false;
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(value_node, 0U));
    }
    if (boolean_literal_magnitude(literal, out_magnitude)) {
        return MYLITE_OK;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal->span, out_magnitude) != MYLITE_OK) {
        *out_parse_overflow = true;
    }
    return MYLITE_OK;
}

static bool convert_year_numeric_magnitude(
    uint64_t magnitude,
    bool is_negative,
    uint32_t *out_year
) {
    if (out_year == NULL) {
        return false;
    }
    if (is_negative && magnitude != 0U) {
        return false;
    }
    if (magnitude == 0U) {
        *out_year = 0U;
        return true;
    }
    if (magnitude <= year_two_digit_high_max) {
        *out_year = (uint32_t)(year_two_digit_high_base + magnitude);
        return true;
    }
    if (magnitude >= year_two_digit_low_min && magnitude <= year_two_digit_max) {
        *out_year = (uint32_t)(year_two_digit_low_base + magnitude);
        return true;
    }
    if (magnitude >= year_minimum_normal && magnitude <= year_maximum) {
        *out_year = (uint32_t)magnitude;
        return true;
    }

    return false;
}

static int make_year_value(
    struct mylite_db *database,
    uint32_t year,
    struct planned_value *out_value
) {
    char text[year_text_length + 1U];
    int written = snprintf(text, sizeof(text), "%04" PRIu32, year);

    if (written != year_text_length) {
        set_runtime_error(database, "failed to format YEAR value");
        return MYLITE_ERROR;
    }
    return copy_text_value(database, text, out_value);
}

static int make_zero_year_value(struct mylite_db *database, struct planned_value *out_value) {
    return make_year_value(database, 0U, out_value);
}

static int make_zero_year_value_with_warning(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool incorrect_integer,
    char *value_text,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (incorrect_integer) {
        rc = append_incorrect_integer_value_warning(
            database,
            value_text == NULL ? "" : value_text,
            column->name,
            row_number
        );
    } else {
        rc = append_out_of_range_warning(database, column->name, row_number);
    }
    free(value_text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_year_value(database, out_value);
}

static bool year_text_is_canonical(const char *text, size_t text_length) {
    uint32_t year = 0U;

    if (convert_year_direct_string(text, text_length, &year) != MYLITE_OK) {
        return false;
    }
    if (text_length != year_text_length) {
        return false;
    }
    if (year == 0U || (year >= year_minimum_normal && year <= year_maximum)) {
        return true;
    }
    return false;
}

static int canonicalize_datetime_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    enum temporal_text_kind kind = TEMPORAL_TEXT_INVALID;
    bool strict = false;

    if (text == NULL || column_name == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        strict = temporal_sql_mode_is_strict(database);
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        set_incorrect_datetime_value_error(database, text, column_name, row_number);
        free(text);
        return MYLITE_ERROR;
    }

    kind = classify_datetime_text(text, text_length);
    strict = temporal_sql_mode_is_strict(database);
    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return assign_text_value(database, text, text_length, out_value);
    case TEMPORAL_TEXT_FULL_ZERO:
        if (!temporal_sql_mode_has_no_zero_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (!temporal_sql_mode_has_no_zero_in_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        if (temporal_sql_mode_has_allow_invalid_dates(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_INVALID:
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_DEFERRED:
        break;
    }

    set_incorrect_datetime_value_error(database, text, column_name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static bool temporal_sql_mode_is_strict(const struct mylite_db *database) {
    return session_sql_mode_is_strict(database);
}

static bool session_sql_mode_is_strict(const struct mylite_db *database) {
    if (database == NULL) {
        return false;
    }
    return (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES) ||
            session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES)) !=
           0;
}

static bool temporal_sql_mode_has_no_zero_date(const struct mylite_db *database) {
    return (database != NULL &&
            session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE)) != 0;
}

static bool temporal_sql_mode_has_no_zero_in_date(const struct mylite_db *database) {
    return (database != NULL &&
            session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE)) != 0;
}

static bool temporal_sql_mode_has_allow_invalid_dates(const struct mylite_db *database) {
    return (
               database != NULL &&
               session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ALLOW_INVALID_DATES)
           ) != 0;
}

static enum temporal_text_kind classify_date_text(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;

    if (!date_text_has_canonical_shape(text, text_length)) {
        return TEMPORAL_TEXT_INVALID;
    }
    if (date_text_is_zero_date(text, text_length)) {
        return TEMPORAL_TEXT_FULL_ZERO;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day)) {
        return TEMPORAL_TEXT_INVALID;
    }

    return classify_temporal_date_components(year, month, day);
}

static enum temporal_text_kind classify_datetime_text(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;

    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return TEMPORAL_TEXT_INVALID;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        return TEMPORAL_TEXT_FULL_ZERO;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day) ||
        !date_component_text_to_u32(
            text + datetime_hour_text_offset,
            datetime_hour_text_length,
            &hour
        ) ||
        !date_component_text_to_u32(
            text + datetime_minute_text_offset,
            datetime_minute_text_length,
            &minute
        ) ||
        !date_component_text_to_u32(
            text + datetime_second_text_offset,
            datetime_second_text_length,
            &second
        )) {
        return TEMPORAL_TEXT_INVALID;
    }
    if (!datetime_time_components_valid(hour, minute, second)) {
        return TEMPORAL_TEXT_INVALID;
    }

    return classify_temporal_date_components(year, month, day);
}

static enum temporal_text_kind classify_temporal_date_components(
    uint32_t year,
    uint32_t month,
    uint32_t day
) {
    if (year < date_minimum_year || year > date_maximum_year) {
        return TEMPORAL_TEXT_DEFERRED;
    }
    if (month == 0U || day == 0U) {
        if (month <= date_months_per_year && day <= date_maximum_day_field) {
            return TEMPORAL_TEXT_PARTIAL_ZERO;
        }
        return TEMPORAL_TEXT_INVALID;
    }
    if (date_year_month_day_valid(year, month, day)) {
        return TEMPORAL_TEXT_NORMAL;
    }
    if (month >= date_first_month && month <= date_months_per_year &&
        day <= date_maximum_day_field) {
        return TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE;
    }
    return TEMPORAL_TEXT_INVALID;
}

static int make_date_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_date_value(database, out_value);
}

static int make_datetime_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_datetime_value(database, out_value);
}

static int make_timestamp_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_timestamp_value(database, out_value);
}

static bool date_text_has_canonical_shape(const char *text, size_t text_length) {
    if (text == NULL || text_length != date_text_length ||
        text[date_first_separator_offset] != '-' || text[date_second_separator_offset] != '-') {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (index == date_first_separator_offset || index == date_second_separator_offset) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }
    return true;
}

static bool date_text_is_zero_date(const char *text, size_t text_length) {
    static const char zero_date[] = "0000-00-00";

    return (text != NULL && text_length == date_text_length &&
            memcmp(text, zero_date, date_text_length) == 0) != 0;
}

static bool time_text_is_canonical_valid(const char *text, size_t text_length) {
    int64_t seconds = 0;

    return time_text_to_seconds(text, text_length, &seconds);
}

static bool time_text_has_canonical_shape(const char *text, size_t text_length) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (text == NULL || text_length < time_text_minimum_length ||
        text_length > time_text_maximum_length) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    if (is_negative && text_length == time_text_minimum_length) {
        return false;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (first_separator <= hour_offset || text[first_separator] != ':' ||
        text[second_separator] != ':') {
        return false;
    }
    if (first_separator - hour_offset < 2U || first_separator - hour_offset > 3U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if ((index == 0U && is_negative) || index == first_separator || index == second_separator) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }

    return true;
}

static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t hour_digit_count = 0U;

    if (hour == NULL || !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }

    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    hour_digit_count = first_separator - hour_offset;

    return (hour_digit_count == 2U ||
            (hour_digit_count == 3U && *hour >= time_minimum_three_digit_hour)) != 0;
}

static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds) {
    bool is_negative = false;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int64_t total = 0;

    if (out_seconds == NULL ||
        !time_text_to_components(text, text_length, &is_negative, &hour, &minute, &second)) {
        return false;
    }
    if (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second) {
        return false;
    }
    if (!time_text_uses_canonical_hour_width(text, text_length, &hour)) {
        return false;
    }
    if (hour == 0U && minute == 0U && second == 0U && is_negative) {
        return false;
    }
    if (hour > time_maximum_hour ||
        (hour == time_maximum_hour &&
         (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second))) {
        return false;
    }

    total = ((int64_t)hour * time_second_per_hour) + ((int64_t)minute * time_second_per_minute) +
            (int64_t)second;
    if (is_negative) {
        total = -total;
    }
    *out_seconds = total;
    return true;
}

static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (out_is_negative == NULL || out_hour == NULL || out_minute == NULL || out_second == NULL ||
        !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (!date_component_text_to_u32(text + hour_offset, first_separator - hour_offset, out_hour) ||
        !date_component_text_to_u32(text + first_separator + 1U, 2U, out_minute) ||
        !date_component_text_to_u32(text + second_separator + 1U, 2U, out_second)) {
        return false;
    }

    *out_is_negative = is_negative;
    return true;
}

static bool datetime_text_is_canonical_valid(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;

    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        return false;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day) ||
        !date_component_text_to_u32(
            text + datetime_hour_text_offset,
            datetime_hour_text_length,
            &hour
        ) ||
        !date_component_text_to_u32(
            text + datetime_minute_text_offset,
            datetime_minute_text_length,
            &minute
        ) ||
        !date_component_text_to_u32(
            text + datetime_second_text_offset,
            datetime_second_text_length,
            &second
        )) {
        return false;
    }

    return (date_year_month_day_valid(year, month, day) &&
            datetime_time_components_valid(hour, minute, second)) != 0;
}

static bool timestamp_text_is_canonical_valid(const char *text, size_t text_length) {
    return (datetime_text_is_canonical_valid(text, text_length) &&
            timestamp_text_in_supported_range(text, text_length)) != 0;
}

static bool timestamp_text_in_supported_range(const char *text, size_t text_length) {
    static const char minimum_timestamp[] = "1970-01-01 00:00:01";
    static const char maximum_timestamp[] = "2038-01-19 03:14:07";

    return (text != NULL && text_length == datetime_text_length &&
            memcmp(text, minimum_timestamp, datetime_text_length) >= 0 &&
            memcmp(text, maximum_timestamp, datetime_text_length) <= 0) != 0;
}

static bool datetime_text_has_canonical_shape(const char *text, size_t text_length) {
    if (text == NULL || text_length != datetime_text_length ||
        text[date_first_separator_offset] != '-' || text[date_second_separator_offset] != '-' ||
        text[datetime_date_time_separator_offset] != ' ' ||
        text[datetime_first_time_separator_offset] != ':' ||
        text[datetime_second_time_separator_offset] != ':') {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (index == date_first_separator_offset || index == date_second_separator_offset ||
            index == datetime_date_time_separator_offset ||
            index == datetime_first_time_separator_offset ||
            index == datetime_second_time_separator_offset) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }
    return true;
}

static bool datetime_text_is_zero_datetime(const char *text, size_t text_length) {
    static const char zero_datetime[] = "0000-00-00 00:00:00";

    return (text != NULL && text_length == datetime_text_length &&
            memcmp(text, zero_datetime, datetime_text_length) == 0) != 0;
}

static bool datetime_time_components_valid(uint32_t hour, uint32_t minute, uint32_t second) {
    return (hour <= datetime_maximum_hour && minute <= datetime_maximum_minute_or_second &&
            second <= datetime_maximum_minute_or_second) != 0;
}

static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value) {
    uint32_t value = 0U;

    if (text == NULL || length == 0U || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        value = (value * date_decimal_base) + (uint32_t)(byte - '0');
    }

    *out_value = value;
    return true;
}

static bool date_year_month_day_valid(uint32_t year, uint32_t month, uint32_t day) {
    static const uint32_t month_days[] = {
        31U,
        28U,
        31U,
        30U,
        31U,
        30U,
        31U,
        31U,
        30U,
        31U,
        30U,
        31U,
    };
    uint32_t maximum_day = 0U;

    if (year < date_minimum_year || year > date_maximum_year || month < date_first_month ||
        month > date_months_per_year || day < date_first_day) {
        return false;
    }
    maximum_day = month_days[month - 1U];
    if (month == date_february && date_year_is_leap(year)) {
        maximum_day = date_leap_day;
    }

    return day <= maximum_day;
}

static bool date_year_is_leap(uint32_t year) {
    if (year % date_leap_year_quadricentennial == 0U) {
        return true;
    }
    if (year % date_leap_year_century == 0U) {
        return false;
    }
    return year % date_leap_year_quadrennial == 0U;
}

static int parse_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *operand_name,
    bool *out_is_negative,
    uint64_t *out_magnitude
) {
    const struct mylite_sql_ast_node *literal = value_node;
    int rc = MYLITE_OK;

    *out_is_negative = false;
    *out_magnitude = 0U;
    if (value_node == NULL) {
        set_unsupported_error(database, "HAVING supports only integer or boolean literals");
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "HAVING supports only integer or boolean literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, out_magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(database, "HAVING supports only integer or boolean literals");
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, out_magnitude);
        if (rc != MYLITE_OK) {
            set_having_out_of_range_error(database, operand_name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int convert_grouped_having_group_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};

    if (column == NULL) {
        return MYLITE_ERROR;
    }

    int rc = integer_range_for_column(
        database,
        column,
        "HAVING supports only integer grouped columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_having_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (magnitude == int64_negative_abs_max) {
            out_value->integer = INT64_MIN;
        } else {
            out_value->integer = -(int64_t)magnitude;
        }
        out_value->is_null = false;
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_having_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    out_value->integer = (int64_t)magnitude;
    out_value->is_null = false;
    return MYLITE_OK;
}

static int convert_grouped_having_aggregate_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const char *operand_name,
    struct planned_value *out_value
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;

    if (is_negative && magnitude > int64_negative_abs_max) {
        set_having_out_of_range_error(database, operand_name);
        return MYLITE_ERROR;
    }
    if (!is_negative && magnitude > int64_positive_max) {
        set_having_out_of_range_error(database, operand_name);
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude == int64_negative_abs_max) {
        out_value->integer = INT64_MIN;
    } else if (is_negative) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    out_value->is_null = false;

    return MYLITE_OK;
}
