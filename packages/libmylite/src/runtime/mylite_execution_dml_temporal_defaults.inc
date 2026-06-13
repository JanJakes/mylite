static int make_zero_date_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "0000-00-00", out_value);
}

static int make_zero_time_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "00:00:00", out_value);
}

static int make_zero_datetime_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "0000-00-00 00:00:00", out_value);
}

static int make_zero_timestamp_value(struct mylite_db *database, struct planned_value *out_value) {
    return make_zero_datetime_value(database, out_value);
}

static int make_current_timestamp_value(
    struct mylite_db *database,
    struct planned_value *out_value
) {
    char timestamp_text[datetime_text_length + 1U];
    int rc = format_current_timestamp_text(database, timestamp_text, sizeof(timestamp_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, timestamp_text, out_value);
}

static int make_sysdate_value(struct mylite_db *database, struct planned_value *out_value) {
    char timestamp_text[datetime_text_length + 1U];
    int rc = format_sysdate_text(database, timestamp_text, sizeof(timestamp_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, timestamp_text, out_value);
}

static int make_current_date_value(struct mylite_db *database, struct planned_value *out_value) {
    char date_text[date_text_length + 1U];
    int rc = format_current_date_text(database, date_text, sizeof(date_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, date_text, out_value);
}

static int make_current_time_value(struct mylite_db *database, struct planned_value *out_value) {
    char time_text[time_text_minimum_length + 1U];
    int rc = format_current_time_text(database, time_text, sizeof(time_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, time_text, out_value);
}

static int make_utc_timestamp_value(struct mylite_db *database, struct planned_value *out_value) {
    char timestamp_text[datetime_text_length + 1U];
    int rc = format_utc_timestamp_text(database, timestamp_text, sizeof(timestamp_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, timestamp_text, out_value);
}

static int make_utc_date_value(struct mylite_db *database, struct planned_value *out_value) {
    char date_text[date_text_length + 1U];
    int rc = format_utc_date_text(database, date_text, sizeof(date_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, date_text, out_value);
}

static int make_utc_time_value(struct mylite_db *database, struct planned_value *out_value) {
    char time_text[time_text_minimum_length + 1U];
    int rc = format_utc_time_text(database, time_text, sizeof(time_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, time_text, out_value);
}

static int64_t current_timestamp_epoch(const struct mylite_db *database) {
    time_t fallback = 0;

    if (database != NULL && database->session.has_timestamp_override) {
        return database->session.timestamp_override;
    }
    if (database != NULL && database->session.active_statement_time > 0) {
        return database->session.active_statement_time;
    }
    fallback = time(NULL);
    if (fallback < (time_t)0) {
        return 0;
    }
    return (int64_t)fallback;
}

static int format_current_timestamp_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid current timestamp buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_sysdate_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    return format_sysdate_text_with_precision(database, 0U, buffer, buffer_size);
}

static int format_sysdate_text_with_precision(
    struct mylite_db *database,
    uint64_t precision,
    char *buffer,
    size_t buffer_size
) {
    enum {
        sysdate_fractional_second_precision_max = 6U,
        decimal_radix = 10,
    };
    struct wall_clock_timestamp timestamp = {0};
    struct tm time_parts;
    long fractional = 0;
    long divisor = 1;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid SYSDATE buffer");
        return MYLITE_ERROR;
    }
    if (precision > sysdate_fractional_second_precision_max) {
        set_temporal_precision_too_big_error(database, "sysdate", precision);
        return MYLITE_ERROR;
    }

    timestamp = current_wall_clock_timestamp();
    if (session_time_parts_from_epoch(database, timestamp.epoch, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    if (precision == 0U) {
        written = snprintf(
            buffer,
            buffer_size,
            "%04d-%02d-%02d %02d:%02d:%02d",
            time_parts.tm_year + tm_year_calendar_offset,
            time_parts.tm_mon + 1,
            time_parts.tm_mday,
            time_parts.tm_hour,
            time_parts.tm_min,
            time_parts.tm_sec
        );
    } else {
        for (uint64_t index = precision; index < sysdate_fractional_second_precision_max; ++index) {
            divisor *= decimal_radix;
        }
        fractional = timestamp.microsecond / divisor;
        written = snprintf(
            buffer,
            buffer_size,
            "%04d-%02d-%02d %02d:%02d:%02d.%0*ld",
            time_parts.tm_year + tm_year_calendar_offset,
            time_parts.tm_mon + 1,
            time_parts.tm_mday,
            time_parts.tm_hour,
            time_parts.tm_min,
            time_parts.tm_sec,
            (int)precision,
            fractional
        );
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format SYSDATE value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_current_date_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid current date buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format current date");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_current_time_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid current time buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%02d:%02d:%02d",
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format current time");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_utc_timestamp_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid UTC timestamp buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_utc_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_utc_date_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid UTC date buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_utc_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format UTC date");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_utc_time_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid UTC time buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_utc_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%02d:%02d:%02d",
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format UTC time");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int current_timestamp_session_parts(struct mylite_db *database, struct tm *out_time_parts) {
    return session_time_parts_from_epoch(
        database,
        current_timestamp_epoch(database),
        out_time_parts
    );
}

static int current_timestamp_utc_parts(struct mylite_db *database, struct tm *out_time_parts) {
    return utc_time_parts_from_epoch(database, current_timestamp_epoch(database), out_time_parts);
}

static int format_session_timestamp_epoch_text(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size
) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid timestamp metadata buffer");
        return MYLITE_ERROR;
    }
    if (session_time_parts_from_epoch(database, epoch, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format timestamp metadata");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int session_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
) {
    int offset_seconds = database == NULL ? 0
                                          : database->session.time_zone_offset_minutes *
                                                time_zone_seconds_per_minute;
    time_t time_value = (time_t)0;

    if (out_time_parts == NULL) {
        return MYLITE_MISUSE;
    }
    if ((offset_seconds > 0 && epoch > INT64_MAX - (int64_t)offset_seconds) ||
        (offset_seconds < 0 && epoch < INT64_MIN - (int64_t)offset_seconds)) {
        set_runtime_error(database, "failed to apply session time zone");
        return MYLITE_ERROR;
    }
    epoch += (int64_t)offset_seconds;
    time_value = (time_t)epoch;
    if ((int64_t)time_value != epoch) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
#ifdef _WIN32
    if (gmtime_s(out_time_parts, &time_value) != 0) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
#else
    if (gmtime_r(&time_value, out_time_parts) == NULL) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
#endif
    return MYLITE_OK;
}

static int utc_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
) {
    time_t time_value = (time_t)0;

    if (out_time_parts == NULL) {
        return MYLITE_MISUSE;
    }
    time_value = (time_t)epoch;
    if ((int64_t)time_value != epoch) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
#ifdef _WIN32
    if (gmtime_s(out_time_parts, &time_value) != 0) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
#else
    if (gmtime_r(&time_value, out_time_parts) == NULL) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
#endif
    return MYLITE_OK;
}

static struct wall_clock_timestamp current_wall_clock_timestamp(void) {
    enum { nanoseconds_per_microsecond = 1000L };
    struct wall_clock_timestamp timestamp = {0};
#ifdef TIME_UTC
    struct timespec now;

    if (timespec_get(&now, TIME_UTC) == TIME_UTC && now.tv_sec >= (time_t)0) {
        timestamp.epoch = (int64_t)now.tv_sec;
        timestamp.microsecond = now.tv_nsec / nanoseconds_per_microsecond;
        return timestamp;
    }
#endif
    {
        time_t fallback_now = time(NULL);

        if (fallback_now >= (time_t)0) {
            timestamp.epoch = (int64_t)fallback_now;
        }
    }
    return timestamp;
}

static int64_t current_wall_clock_epoch(void) {
    struct wall_clock_timestamp timestamp = current_wall_clock_timestamp();

    return timestamp.epoch;
}
