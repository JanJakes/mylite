static bool normalize_relaxed_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
);
static bool parse_relaxed_date_prefix(
    const char *text,
    size_t text_length,
    size_t *inout_offset,
    uint32_t *out_year,
    uint32_t *out_month,
    uint32_t *out_day
);
static bool parse_temporal_digits(
    const char *text,
    size_t text_length,
    size_t *inout_offset,
    size_t minimum_digits,
    size_t maximum_digits,
    uint32_t *out_value
);
static bool parse_temporal_char(
    const char *text,
    size_t text_length,
    size_t *inout_offset,
    char expected
);

static int convert_predicate_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool force_datetime_comparison,
    struct planned_value *out_value,
    bool *out_compare_as_datetime
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    bool append_warning = false;
    int rc = MYLITE_OK;

    if (out_compare_as_datetime != NULL) {
        *out_compare_as_datetime = false;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE DATE predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE DATE predicates support only string literals",
        "WHERE DATE predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_date_text_admitted(database, text, text_length)) {
        if (force_datetime_comparison) {
            rc = copy_date_midnight_predicate_value(database, text, out_value);
            free(text);
            return rc;
        }
        return assign_text_value(database, text, text_length, out_value);
    }

    if (normalize_date_datetime_predicate_text(text, text_length, normalized, &append_warning)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_date_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        if (append_warning) {
            rc = append_incorrect_date_predicate_warning(database, text, column->name);
            if (rc != MYLITE_OK) {
                free(text);
                return rc;
            }
        }
        if (out_compare_as_datetime != NULL) {
            *out_compare_as_datetime = true;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }

    set_incorrect_date_literal_error(database, text);
    free(text);
    return MYLITE_ERROR;
}

static bool normalize_date_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text,
    bool *out_append_warning
) {
    int ignored_offset_minutes = 0;

    if (text == NULL || out_text == NULL || out_append_warning == NULL) {
        return false;
    }
    *out_append_warning = false;
    if (text_length == datetime_text_length && (text[datetime_date_time_separator_offset] == ' ' ||
                                                text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }
    if (text_length == datetime_text_length + temporal_predicate_offset_text_length &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        if (!datetime_text_has_canonical_shape(out_text, datetime_text_length)) {
            return false;
        }
        return parse_temporal_predicate_offset(
            text + datetime_text_length,
            &ignored_offset_minutes
        );
    }
    if (text_length == datetime_text_length + 1U &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T') &&
        (text[datetime_text_length] == 'Z' || text[datetime_text_length] == 'z')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        *out_append_warning = true;
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }

    return false;
}

static int copy_date_midnight_predicate_value(
    struct mylite_db *database,
    const char *date_text,
    struct planned_value *out_value
) {
    char text[datetime_text_length + 1U];

    if (date_text == NULL) {
        return MYLITE_MISUSE;
    }
    memcpy(text, date_text, date_text_length);
    memcpy(text + date_text_length, " 00:00:00", datetime_text_length - date_text_length);
    text[datetime_text_length] = '\0';
    return copy_text_value(database, text, out_value);
}

static int convert_predicate_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int64_t seconds = 0;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE TIME predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE TIME predicates support only string literals",
        "WHERE TIME predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!time_text_to_seconds(text, text_length, &seconds)) {
        set_incorrect_time_value_error(database, text, column->name, 1U);
        free(text);
        return MYLITE_ERROR;
    }
    free(text);

    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = seconds};
    return MYLITE_OK;
}

static int convert_predicate_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE DATETIME predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE DATETIME predicates support only string literals",
        "WHERE DATETIME predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        rc = append_incorrect_datetime_predicate_warning(database, text, column->name);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_relaxed_datetime_predicate_text(text, text_length, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (!predicate_datetime_text_admitted(database, text, text_length)) {
        set_incorrect_datetime_literal_error(database, text);
        free(text);
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static bool normalize_relaxed_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
) {
    size_t offset = 0U;
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int written = 0;

    if (text == NULL || out_text == NULL) {
        return false;
    }
    if (!parse_relaxed_date_prefix(text, text_length, &offset, &year, &month, &day)) {
        return false;
    }
    if (offset != text_length) {
        if (text[offset] != ' ' && text[offset] != 'T') {
            return false;
        }
        ++offset;
        if (!parse_temporal_digits(text, text_length, &offset, 2U, 2U, &hour) ||
            !parse_temporal_char(text, text_length, &offset, ':') ||
            !parse_temporal_digits(text, text_length, &offset, 2U, 2U, &minute) ||
            !parse_temporal_char(text, text_length, &offset, ':') ||
            !parse_temporal_digits(text, text_length, &offset, 2U, 2U, &second) ||
            offset != text_length) {
            return false;
        }
    }

    written = snprintf(
        out_text,
        datetime_text_length + 1U,
        "%04" PRIu32 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        year,
        month,
        day,
        hour,
        minute,
        second
    );
    return written == datetime_text_length;
}

static bool parse_relaxed_date_prefix(
    const char *text,
    size_t text_length,
    size_t *inout_offset,
    uint32_t *out_year,
    uint32_t *out_month,
    uint32_t *out_day
) {
    return (parse_temporal_digits(text, text_length, inout_offset, 4U, 4U, out_year) &&
            parse_temporal_char(text, text_length, inout_offset, '-') &&
            parse_temporal_digits(text, text_length, inout_offset, 1U, 2U, out_month) &&
            parse_temporal_char(text, text_length, inout_offset, '-') &&
            parse_temporal_digits(text, text_length, inout_offset, 1U, 2U, out_day)) != 0;
}

static bool parse_temporal_digits(
    const char *text,
    size_t text_length,
    size_t *inout_offset,
    size_t minimum_digits,
    size_t maximum_digits,
    uint32_t *out_value
) {
    size_t offset = 0U;
    size_t digit_count = 0U;
    uint32_t value = 0U;

    if (text == NULL || inout_offset == NULL || out_value == NULL ||
        minimum_digits > maximum_digits) {
        return false;
    }
    offset = *inout_offset;
    while (offset + digit_count < text_length && digit_count < maximum_digits) {
        unsigned char byte = (unsigned char)text[offset + digit_count];

        if (byte < '0' || byte > '9') {
            break;
        }
        value = (value * decimal_base) + (uint32_t)(byte - '0');
        ++digit_count;
    }
    if (digit_count < minimum_digits) {
        return false;
    }

    *inout_offset = offset + digit_count;
    *out_value = value;
    return true;
}

static bool parse_temporal_char(
    const char *text,
    size_t text_length,
    size_t *inout_offset,
    char expected
) {
    if (text == NULL || inout_offset == NULL || *inout_offset >= text_length ||
        text[*inout_offset] != expected) {
        return false;
    }
    ++(*inout_offset);
    return true;
}

static int convert_predicate_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE TIMESTAMP predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE TIMESTAMP predicates support only string literals",
        "WHERE TIMESTAMP predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = 0;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        if (!predicate_timestamp_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_timestamp_value_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!predicate_timestamp_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_timestamp_value_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        rc = append_incorrect_datetime_predicate_warning(database, text, column->name);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (!predicate_timestamp_text_admitted(database, text, text_length)) {
        set_incorrect_timestamp_value_error(database, text);
        free(text);
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static bool normalize_iso_temporal_predicate_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
) {
    struct mylite_temporal_datetime_parts parts = {0};
    int source_offset_minutes = 0;

    if (input == NULL || input->text == NULL || out_text == NULL) {
        return false;
    }
    if (input->text_length == datetime_text_length &&
        input->text[datetime_date_time_separator_offset] == 'T') {
        memcpy(out_text, input->text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }
    if (input->text_length != datetime_text_length + sizeof("+00:00") - 1U ||
        (input->text[datetime_date_time_separator_offset] != ' ' &&
         input->text[datetime_date_time_separator_offset] != 'T')) {
        return false;
    }

    memcpy(out_text, input->text, datetime_text_length);
    out_text[datetime_date_time_separator_offset] = ' ';
    out_text[datetime_text_length] = '\0';
    if (!mylite_temporal_arithmetic_parse_datetime_text(out_text, datetime_text_length, &parts)) {
        return false;
    }
    if (!parse_temporal_predicate_offset(
            input->text + datetime_text_length,
            &source_offset_minutes
        )) {
        return false;
    }

    return shift_datetime_parts_by_minutes(
        &parts,
        input->target_offset_minutes - source_offset_minutes,
        out_text
    );
}

static bool normalize_z_temporal_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
) {
    if (text == NULL || out_text == NULL) {
        return false;
    }
    if (text_length != datetime_text_length + 1U ||
        (text[datetime_date_time_separator_offset] != ' ' &&
         text[datetime_date_time_separator_offset] != 'T') ||
        (text[datetime_text_length] != 'Z' && text[datetime_text_length] != 'z')) {
        return false;
    }

    memcpy(out_text, text, datetime_text_length);
    out_text[datetime_date_time_separator_offset] = ' ';
    out_text[datetime_text_length] = '\0';
    return datetime_text_has_canonical_shape(out_text, datetime_text_length);
}

static bool parse_temporal_predicate_offset(const char *text, int *out_offset_minutes) {
    int sign = 0;
    int hours = 0;
    int minutes = 0;
    int total_minutes = 0;

    if (text == NULL || out_offset_minutes == NULL) {
        return false;
    }
    if (text[temporal_predicate_offset_sign_index] == '+') {
        sign = 1;
    } else if (text[temporal_predicate_offset_sign_index] == '-') {
        sign = -1;
    } else {
        return false;
    }
    if (text[temporal_predicate_offset_hour_tens_index] < '0' ||
        text[temporal_predicate_offset_hour_tens_index] > '9' ||
        text[temporal_predicate_offset_hour_ones_index] < '0' ||
        text[temporal_predicate_offset_hour_ones_index] > '9' ||
        text[temporal_predicate_offset_separator_index] != ':' ||
        text[temporal_predicate_offset_minute_tens_index] < '0' ||
        text[temporal_predicate_offset_minute_tens_index] > '9' ||
        text[temporal_predicate_offset_minute_ones_index] < '0' ||
        text[temporal_predicate_offset_minute_ones_index] > '9' ||
        text[temporal_predicate_offset_text_length] != '\0') {
        return false;
    }

    hours = ((text[temporal_predicate_offset_hour_tens_index] - '0') * decimal_base) +
            (text[temporal_predicate_offset_hour_ones_index] - '0');
    minutes = ((text[temporal_predicate_offset_minute_tens_index] - '0') * decimal_base) +
              (text[temporal_predicate_offset_minute_ones_index] - '0');
    if (minutes > time_zone_max_minute) {
        return false;
    }
    total_minutes = (hours * time_zone_minutes_per_hour) + minutes;
    if (total_minutes > temporal_predicate_offset_max_minutes) {
        return false;
    }
    if (sign < 0 && total_minutes == 0) {
        return false;
    }

    *out_offset_minutes = sign * total_minutes;
    return true;
}

static bool shift_datetime_parts_by_minutes(
    const struct mylite_temporal_datetime_parts *parts,
    int delta_minutes,
    char *out_text
) {
    const int64_t seconds_per_day = mylite_temporal_arithmetic_seconds_per_day();
    struct mylite_temporal_day_second shifted = {0};
    struct mylite_temporal_datetime_parts output = {0};
    int64_t days = 0;
    int64_t day_seconds = 0;
    int64_t base_seconds = 0;
    int64_t shifted_seconds = 0;
    int64_t result_day_seconds = 0;

    if (parts == NULL || out_text == NULL) {
        return false;
    }

    days = mylite_temporal_arithmetic_days_from_datetime(parts);
    day_seconds = ((int64_t)parts->hour * (int64_t)time_second_per_hour) +
                  ((int64_t)parts->minute * (int64_t)time_second_per_minute) +
                  (int64_t)parts->second;
    base_seconds = (days * seconds_per_day) + day_seconds;
    if (!mylite_temporal_arithmetic_checked_add_int64(
            base_seconds,
            (int64_t)delta_minutes * time_zone_seconds_per_minute,
            &shifted_seconds
        )) {
        return false;
    }

    shifted = mylite_temporal_arithmetic_floor_divmod_seconds(shifted_seconds);
    mylite_temporal_arithmetic_civil_from_days(shifted.days, &output);
    if (output.year < date_minimum_year || output.year > date_maximum_year) {
        return false;
    }
    result_day_seconds = shifted.day_seconds;
    output.hour = (uint32_t)(result_day_seconds / time_second_per_hour);
    result_day_seconds %= time_second_per_hour;
    output.minute = (uint32_t)(result_day_seconds / time_second_per_minute);
    output.second = (uint32_t)(result_day_seconds % time_second_per_minute);

    return format_datetime_parts_text(&output, out_text);
}

static bool format_datetime_parts_text(
    const struct mylite_temporal_datetime_parts *parts,
    char *out_text
) {
    int written = 0;

    if (parts == NULL || out_text == NULL) {
        return false;
    }

    written = snprintf(
        out_text,
        datetime_text_length + 1U,
        "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        parts->year,
        parts->month,
        parts->day,
        parts->hour,
        parts->minute,
        parts->second
    );
    return written == datetime_text_length;
}

static bool predicate_date_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    enum temporal_text_kind kind = classify_date_text(text, text_length);

    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return true;
    case TEMPORAL_TEXT_FULL_ZERO:
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (temporal_sql_mode_has_no_zero_in_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        return temporal_sql_mode_has_allow_invalid_dates(database);
    case TEMPORAL_TEXT_INVALID:
    case TEMPORAL_TEXT_DEFERRED:
        return false;
    }
    return false;
}

static bool predicate_datetime_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    enum temporal_text_kind kind = classify_datetime_text(text, text_length);

    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return true;
    case TEMPORAL_TEXT_FULL_ZERO:
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (temporal_sql_mode_has_no_zero_in_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        return temporal_sql_mode_has_allow_invalid_dates(database);
    case TEMPORAL_TEXT_INVALID:
    case TEMPORAL_TEXT_DEFERRED:
        return false;
    }
    return false;
}

static bool predicate_timestamp_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    if (timestamp_text_is_canonical_valid(text, text_length)) {
        return true;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    }
    return false;
}

static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "WHERE supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if (magnitude > bigint_signed_negative_abs_max) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (range.negative_abs_max != 0U && magnitude > range.negative_abs_max) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_predicate_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}
