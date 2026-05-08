#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mylite_dml_temporal_kind {
    MYLITE_DML_TEMPORAL_NONE = 0,
    MYLITE_DML_TEMPORAL_DATE,
    MYLITE_DML_TEMPORAL_DATETIME,
    MYLITE_DML_TEMPORAL_TIMESTAMP,
};

enum mylite_dml_temporal_problem {
    MYLITE_DML_TEMPORAL_PROBLEM_NONE = 0,
    MYLITE_DML_TEMPORAL_PROBLEM_MALFORMED,
    MYLITE_DML_TEMPORAL_PROBLEM_CALENDAR,
    MYLITE_DML_TEMPORAL_PROBLEM_ZERO_DATE,
    MYLITE_DML_TEMPORAL_PROBLEM_ZERO_IN_DATE,
};

enum {
    MYLITE_DML_TEMPORAL_TEXT_BUFFER_SIZE = 64,
};

struct mylite_dml_temporal_parts {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    bool has_time;
};

struct mylite_dml_temporal_output {
    char text[sizeof("0000-00-00 00:00:00")];
    size_t length;
    bool replace;
};

static int coerce_temporal_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_temporal_output *out_output
);

static enum mylite_dml_temporal_kind temporal_kind_for_column(
    const struct mylite_insert_table_column *column
);

static bool parse_temporal_parts(
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    struct mylite_dml_temporal_parts *out_parts
);

static bool parse_compact_temporal_parts(
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    struct mylite_dml_temporal_parts *out_parts
);

static bool parse_delimited_temporal_parts(
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    struct mylite_dml_temporal_parts *out_parts
);

static bool parse_four_digits(const char *text, int *out_value);

static bool parse_two_digits(const char *text, int *out_value);

static bool text_is_digits(const char *text, size_t text_length);

static enum mylite_dml_temporal_problem temporal_problem_for_parts(
    const struct mylite_dml_temporal_parts *parts
);

static enum mylite_dml_temporal_problem temporal_problem_for_modes(
    const mylite_db *database,
    enum mylite_dml_temporal_kind kind,
    enum mylite_dml_temporal_problem problem
);

static bool temporal_parts_are_zero_date(const struct mylite_dml_temporal_parts *parts);

static bool temporal_parts_are_zero_in_date(const struct mylite_dml_temporal_parts *parts);

static bool temporal_date_is_calendar_valid(const struct mylite_dml_temporal_parts *parts);

static int temporal_month_day_limit(int year, int month);

static bool temporal_year_is_leap(int year);

static void set_zero_temporal_output(
    enum mylite_dml_temporal_kind kind,
    struct mylite_dml_temporal_output *out_output
);

static int set_valid_temporal_output(struct mylite_dml_temporal_output *out_output);

static bool temporal_text_matches_output(
    const char *text,
    size_t text_length,
    const struct mylite_dml_temporal_output *output
);

static int set_temporal_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number
);

static int append_temporal_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_temporal_problem problem,
    uint64_t row_number
);

static int replace_insert_temporal_text(
    mylite_db *database,
    const struct mylite_dml_temporal_output *output,
    struct mylite_insert_bound_value *value
);

static int replace_insert_temporal_text_with_copy(
    mylite_db *database,
    const char *text,
    size_t text_length,
    struct mylite_insert_bound_value *value
);

static int copy_insert_temporal_value_text(
    mylite_db *database,
    const struct mylite_insert_bound_value *value,
    char **out_text,
    size_t *out_length
);

static int replace_update_temporal_text(
    mylite_db *database,
    const struct mylite_dml_temporal_output *output,
    struct mylite_expression_value *value
);

int mylite_dml_coerce_insert_temporal_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
) {
    enum mylite_dml_temporal_kind kind = temporal_kind_for_column(column);
    struct mylite_dml_temporal_output output = {0};
    char *converted_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    int status = MYLITE_OK;

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_TEMPORAL_NONE || value->kind == MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }

    if (value->kind == MYLITE_INSERT_BOUND_TEXT) {
        text = value->text_value;
        text_length = value->text_length;
    } else {
        status = copy_insert_temporal_value_text(database, value, &converted_text, &text_length);
        if (status != MYLITE_OK) {
            return status;
        }
        text = converted_text;
    }

    status = coerce_temporal_text_value(
        database,
        column,
        kind,
        text,
        text_length,
        row_number,
        ignore,
        &output
    );
    if (status != MYLITE_OK) {
        free(converted_text);
        return status;
    }
    if (output.replace) {
        status = replace_insert_temporal_text(database, &output, value);
        free(converted_text);
        return status;
    }
    if (converted_text != NULL) {
        status = replace_insert_temporal_text_with_copy(database, text, text_length, value);
    }
    free(converted_text);
    return status;
}

int mylite_dml_coerce_update_temporal_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
) {
    enum mylite_dml_temporal_kind kind = temporal_kind_for_column(column);
    struct mylite_dml_temporal_output output = {0};
    char *converted_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    int status = MYLITE_OK;

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_TEMPORAL_NONE || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }

    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        text = value->text_value;
        text_length = value->text_length;
    } else {
        converted_text = mylite_expression_value_to_text(value);
        if (converted_text == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        text = converted_text;
        text_length = strlen(converted_text);
    }

    status = coerce_temporal_text_value(
        database,
        column,
        kind,
        text,
        text_length,
        row_number,
        ignore,
        &output
    );
    free(converted_text);
    if (status != MYLITE_OK || !output.replace) {
        return status;
    }
    return replace_update_temporal_text(database, &output, value);
}

static int coerce_temporal_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_temporal_output *out_output
) {
    struct mylite_dml_temporal_parts parts = {0};
    enum mylite_dml_temporal_problem problem = MYLITE_DML_TEMPORAL_PROBLEM_NONE;

    if (text == NULL || out_output == NULL) {
        return MYLITE_MISUSE;
    }

    if (!parse_temporal_parts(kind, text, text_length, &parts)) {
        problem = MYLITE_DML_TEMPORAL_PROBLEM_MALFORMED;
    } else {
        problem = temporal_problem_for_parts(&parts);
        problem = temporal_problem_for_modes(database, kind, problem);
    }

    if (problem != MYLITE_DML_TEMPORAL_PROBLEM_NONE) {
        if (!ignore && mylite_connection_sql_mode_is_strict(database)) {
            return set_temporal_error(database, column, kind, text, text_length, row_number);
        }
        int status = append_temporal_warning(database, column, problem, row_number);

        if (status != MYLITE_OK) {
            return status;
        }
        set_zero_temporal_output(kind, out_output);
        out_output->replace = !temporal_text_matches_output(text, text_length, out_output);
        return MYLITE_OK;
    }

    return set_valid_temporal_output(out_output);
}

static enum mylite_dml_temporal_kind temporal_kind_for_column(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL || column->data_type == NULL) {
        return MYLITE_DML_TEMPORAL_NONE;
    }
    if (mylite_ascii_case_equal(column->data_type, "date")) {
        return MYLITE_DML_TEMPORAL_DATE;
    }
    if (mylite_ascii_case_equal(column->data_type, "datetime")) {
        return MYLITE_DML_TEMPORAL_DATETIME;
    }
    if (mylite_ascii_case_equal(column->data_type, "timestamp")) {
        return MYLITE_DML_TEMPORAL_TIMESTAMP;
    }
    return MYLITE_DML_TEMPORAL_NONE;
}

static bool parse_temporal_parts(
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    struct mylite_dml_temporal_parts *out_parts
) {
    if (text == NULL || text_length == 0U || out_parts == NULL) {
        return false;
    }
    if (text_is_digits(text, text_length)) {
        return parse_compact_temporal_parts(kind, text, text_length, out_parts);
    }
    return parse_delimited_temporal_parts(kind, text, text_length, out_parts);
}

static bool parse_compact_temporal_parts(
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    struct mylite_dml_temporal_parts *out_parts
) {
    if (text_length != 8U && text_length != 14U) {
        return false;
    }
    if (kind == MYLITE_DML_TEMPORAL_DATE && text_length != 8U) {
        return false;
    }
    if (!parse_four_digits(text, &out_parts->year) ||
        !parse_two_digits(text + 4U, &out_parts->month) ||
        !parse_two_digits(text + 6U, &out_parts->day)) {
        return false;
    }
    if (text_length == 14U) {
        out_parts->has_time = true;
        if (!parse_two_digits(text + 8U, &out_parts->hour) ||
            !parse_two_digits(text + 10U, &out_parts->minute) ||
            !parse_two_digits(text + 12U, &out_parts->second)) {
            return false;
        }
    }
    return true;
}

static bool parse_delimited_temporal_parts(
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    struct mylite_dml_temporal_parts *out_parts
) {
    size_t offset = 0U;

    if (text_length < 10U || text[4] != '-' || text[7] != '-') {
        return false;
    }
    if (!parse_four_digits(text, &out_parts->year) ||
        !parse_two_digits(text + 5U, &out_parts->month) ||
        !parse_two_digits(text + 8U, &out_parts->day)) {
        return false;
    }
    offset = 10U;
    if (offset == text_length) {
        return true;
    }
    if (text[offset] != ' ' && text[offset] != 'T') {
        return false;
    }
    if (text_length < offset + 9U || text[offset + 3U] != ':' || text[offset + 6U] != ':') {
        return false;
    }
    out_parts->has_time = true;
    if (!parse_two_digits(text + offset + 1U, &out_parts->hour) ||
        !parse_two_digits(text + offset + 4U, &out_parts->minute) ||
        !parse_two_digits(text + offset + 7U, &out_parts->second)) {
        return false;
    }
    offset += 9U;
    if (offset == text_length) {
        return true;
    }
    if (text[offset] != '.') {
        return false;
    }
    for (++offset; offset < text_length; ++offset) {
        if (text[offset] < '0' || text[offset] > '9') {
            return false;
        }
    }
    return kind != MYLITE_DML_TEMPORAL_DATE || out_parts->has_time;
}

static bool parse_four_digits(const char *text, int *out_value) {
    if (!parse_two_digits(text, out_value)) {
        return false;
    }
    int low = 0;

    if (!parse_two_digits(text + 2U, &low)) {
        return false;
    }
    *out_value = (*out_value * 100) + low;
    return true;
}

static bool parse_two_digits(const char *text, int *out_value) {
    if (text == NULL || out_value == NULL || text[0] < '0' || text[0] > '9' || text[1] < '0' ||
        text[1] > '9') {
        return false;
    }
    *out_value = ((int)(text[0] - '0') * 10) + (int)(text[1] - '0');
    return true;
}

static bool text_is_digits(const char *text, size_t text_length) {
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

static enum mylite_dml_temporal_problem temporal_problem_for_parts(
    const struct mylite_dml_temporal_parts *parts
) {
    if (parts->month > 12 || parts->day > 31 || parts->hour > 23 || parts->minute > 59 ||
        parts->second > 59) {
        return MYLITE_DML_TEMPORAL_PROBLEM_MALFORMED;
    }
    if (temporal_parts_are_zero_date(parts)) {
        return MYLITE_DML_TEMPORAL_PROBLEM_ZERO_DATE;
    }
    if (temporal_parts_are_zero_in_date(parts)) {
        return MYLITE_DML_TEMPORAL_PROBLEM_ZERO_IN_DATE;
    }
    if (!temporal_date_is_calendar_valid(parts)) {
        return MYLITE_DML_TEMPORAL_PROBLEM_CALENDAR;
    }
    return MYLITE_DML_TEMPORAL_PROBLEM_NONE;
}

static enum mylite_dml_temporal_problem temporal_problem_for_modes(
    const mylite_db *database,
    enum mylite_dml_temporal_kind kind,
    enum mylite_dml_temporal_problem problem
) {
    if (problem == MYLITE_DML_TEMPORAL_PROBLEM_CALENDAR && kind != MYLITE_DML_TEMPORAL_TIMESTAMP &&
        mylite_connection_sql_mode_allows_invalid_dates(database)) {
        return MYLITE_DML_TEMPORAL_PROBLEM_NONE;
    }
    if (problem == MYLITE_DML_TEMPORAL_PROBLEM_ZERO_DATE &&
        !mylite_connection_sql_mode_has_no_zero_date(database)) {
        return MYLITE_DML_TEMPORAL_PROBLEM_NONE;
    }
    if (problem == MYLITE_DML_TEMPORAL_PROBLEM_ZERO_IN_DATE) {
        if (kind != MYLITE_DML_TEMPORAL_TIMESTAMP &&
            !mylite_connection_sql_mode_has_no_zero_in_date(database)) {
            return MYLITE_DML_TEMPORAL_PROBLEM_NONE;
        }
    }
    return problem;
}

static bool temporal_parts_are_zero_date(const struct mylite_dml_temporal_parts *parts) {
    return parts != NULL && parts->year == 0 && parts->month == 0 && parts->day == 0;
}

static bool temporal_parts_are_zero_in_date(const struct mylite_dml_temporal_parts *parts) {
    return parts != NULL && !temporal_parts_are_zero_date(parts) &&
           (parts->month == 0 || parts->day == 0);
}

static bool temporal_date_is_calendar_valid(const struct mylite_dml_temporal_parts *parts) {
    int day_limit = parts == NULL ? 0 : temporal_month_day_limit(parts->year, parts->month);

    return day_limit != 0 && parts->day >= 1 && parts->day <= day_limit;
}

static int temporal_month_day_limit(int year, int month) {
    static const int common[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const int leap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < 1 || month > 12) {
        return 0;
    }
    return temporal_year_is_leap(year) ? leap[month - 1] : common[month - 1];
}

static bool temporal_year_is_leap(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || (year % 400) == 0;
}

static void set_zero_temporal_output(
    enum mylite_dml_temporal_kind kind,
    struct mylite_dml_temporal_output *out_output
) {
    const char *zero = kind == MYLITE_DML_TEMPORAL_DATE ? "0000-00-00" : "0000-00-00 00:00:00";

    memcpy(out_output->text, zero, strlen(zero) + 1U);
    out_output->length = strlen(zero);
}

static int set_valid_temporal_output(struct mylite_dml_temporal_output *out_output) {
    out_output->replace = false;
    return MYLITE_OK;
}

static bool temporal_text_matches_output(
    const char *text,
    size_t text_length,
    const struct mylite_dml_temporal_output *output
) {
    return text != NULL && output != NULL && text_length == output->length &&
           memcmp(text, output->text, output->length) == 0;
}

static int set_temporal_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_temporal_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number
) {
    char preview[65];
    size_t preview_length = text_length >= sizeof(preview) ? sizeof(preview) - 1U : text_length;
    const char *value_kind = kind == MYLITE_DML_TEMPORAL_DATE ? "date" : "datetime";
    char *message = NULL;
    int status = MYLITE_OK;

    memcpy(preview, text, preview_length);
    preview[preview_length] = '\0';
    message = sqlite3_mprintf(
        "Incorrect %s value: '%q' for column '%q' at row %llu",
        value_kind,
        preview,
        column->name,
        (unsigned long long)(row_number == 0U ? 1U : row_number)
    );
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_temporal_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_temporal_problem problem,
    uint64_t row_number
) {
    unsigned int code = problem == MYLITE_DML_TEMPORAL_PROBLEM_MALFORMED
                            ? MYLITE_MYSQL_ER_WARN_DATA_TRUNCATED
                            : MYLITE_MYSQL_ER_WARN_DATA_OUT_OF_RANGE;
    const char *format = problem == MYLITE_DML_TEMPORAL_PROBLEM_MALFORMED
                             ? "Data truncated for column '%q' at row %llu"
                             : "Out of range value for column '%q' at row %llu";
    char *message = sqlite3_mprintf(
        format,
        column->name,
        (unsigned long long)(row_number == 0U ? 1U : row_number)
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_warning(database, code, message);
    sqlite3_free(message);
    return status;
}

static int replace_insert_temporal_text(
    mylite_db *database,
    const struct mylite_dml_temporal_output *output,
    struct mylite_insert_bound_value *value
) {
    char *copy = mylite_copy_span_text(output->text, output->length);

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_dml_insert_bound_value_deinit(value);
    *value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_TEXT,
        .text_value = copy,
        .text_length = output->length,
    };
    return MYLITE_OK;
}

static int replace_insert_temporal_text_with_copy(
    mylite_db *database,
    const char *text,
    size_t text_length,
    struct mylite_insert_bound_value *value
) {
    char *copy = mylite_copy_span_text(text, text_length);

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_dml_insert_bound_value_deinit(value);
    *value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_TEXT,
        .text_value = copy,
        .text_length = text_length,
    };
    return MYLITE_OK;
}

static int copy_insert_temporal_value_text(
    mylite_db *database,
    const struct mylite_insert_bound_value *value,
    char **out_text,
    size_t *out_length
) {
    char buffer[MYLITE_DML_TEMPORAL_TEXT_BUFFER_SIZE] = {0};
    int length = 0;

    if (database == NULL || value == NULL || out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }

    *out_text = NULL;
    *out_length = 0U;
    switch (value->kind) {
    case MYLITE_INSERT_BOUND_INTEGER:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->integer_value);
        break;
    case MYLITE_INSERT_BOUND_REAL:
        length = mylite_format_compact_real_text(value->real_value, buffer, sizeof(buffer));
        break;
    case MYLITE_INSERT_BOUND_BLOB:
        *out_text = mylite_copy_span_text(value->text_value, value->text_length);
        if (*out_text == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_length = value->text_length;
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_TEXT:
    case MYLITE_INSERT_BOUND_NULL:
        return MYLITE_MISUSE;
    }

    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_text = mylite_copy_span_text(buffer, (size_t)length);
    if (*out_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int replace_update_temporal_text(
    mylite_db *database,
    const struct mylite_dml_temporal_output *output,
    struct mylite_expression_value *value
) {
    char *copy = mylite_copy_span_text(output->text, output->length);

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_expression_value_deinit(value);
    *value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_TEXT,
        .text_value = copy,
        .text_length = output->length,
    };
    return MYLITE_OK;
}
