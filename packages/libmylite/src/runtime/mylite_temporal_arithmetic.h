#ifndef MYLITE_RUNTIME_MYLITE_TEMPORAL_ARITHMETIC_H
#define MYLITE_RUNTIME_MYLITE_TEMPORAL_ARITHMETIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_temporal_datetime_parts {
    int64_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
};

struct mylite_temporal_day_second {
    int64_t days;
    int64_t day_seconds;
};

bool mylite_temporal_arithmetic_parse_datetime_text(
    const char *text,
    size_t text_length,
    struct mylite_temporal_datetime_parts *out_datetime
);
bool mylite_temporal_arithmetic_checked_add_int64(int64_t left, int64_t right, int64_t *out_value);
bool mylite_temporal_arithmetic_checked_multiply_int64(
    int64_t left,
    int64_t right,
    int64_t *out_value
);
bool mylite_temporal_arithmetic_add_calendar_months(
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_months,
    struct mylite_temporal_datetime_parts *out_datetime
);
int64_t mylite_temporal_arithmetic_seconds_per_day(void);
int64_t mylite_temporal_arithmetic_days_from_datetime(
    const struct mylite_temporal_datetime_parts *datetime
);
void mylite_temporal_arithmetic_civil_from_days(
    int64_t days,
    struct mylite_temporal_datetime_parts *out_datetime
);
struct mylite_temporal_day_second mylite_temporal_arithmetic_floor_divmod_seconds(
    int64_t total_seconds
);

#endif
