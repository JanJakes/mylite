#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
    sysdate_dml_row_count = 5,
    sysdate_two_hour_offset_minutes = 120,
    sysdate_range_tolerance_seconds = 2,
    seconds_per_minute = 60,
    tm_year_calendar_offset = 1900,
    expected_id_capacity = 16,
    timestamp_text_length = 19,
    timestamp_text_capacity = timestamp_text_length + 1,
    timestamp_date_separator_1_index = 4,
    timestamp_date_separator_2_index = 7,
    timestamp_date_time_separator_index = 10,
    timestamp_hour_separator_index = 13,
    timestamp_minute_separator_index = 16,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_sysdate_scalar {
    size_t column;
    int offset_minutes;
};

struct expected_sysdate_range {
    time_t before;
    time_t after;
    int offset_minutes;
};

struct expected_sysdate_table_query {
    const char *sql;
    const char *context;
};

struct timestamp_character_check {
    char value;
    size_t index;
};

static int test_sysdate_scalar_and_do(void);
static int test_sysdate_dml_persistence_and_file_safety(void);
static int test_sysdate_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_scalar_sysdate(
    mylite_db *database,
    const char *sql,
    struct expected_sysdate_scalar expected,
    struct expected_query query
);
static int expect_row_scalar_sysdate(mylite_db *database);
static int expect_sysdate_table_rows(
    mylite_db *database,
    struct expected_sysdate_table_query query
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_result_timestamp_near(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_sysdate_range expected,
    const char *context
);
static int timestamp_text_matches_range(const char *actual, struct expected_sysdate_range expected);
static int format_timestamp_for_epoch(
    int64_t epoch,
    int offset_minutes,
    char *buffer,
    size_t buffer_size
);
static int expect_timestamp_shape(const char *actual, const char *context);
static int timestamp_shape_is_valid(const char *actual);
static int timestamp_character_is_valid(struct timestamp_character_check check);
static int is_ascii_digit(char value);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sysdate_scalar_and_do();
    failures += test_sysdate_dml_persistence_and_file_safety();
    failures += test_sysdate_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_sysdate_scalar_and_do(void) {
    static const char *const timestamp_override_values[] = {
        "2023-11-14 22:13:20",
        NULL,
        "0",
    };
    static const char *const do_counts[] = {"0", "0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar memory");
    failures += expect_statement_ok(database, "SET time_zone = '+00:00'");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_scalar_sysdate(
        database,
        "SELECT NOW(), SYSDATE(), @@warning_count",
        (struct expected_sysdate_scalar){.column = 1U, .offset_minutes = 0},
        (struct expected_query){
            .sql = NULL,
            .values = timestamp_override_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "SYSDATE ignores SET timestamp",
        }
    );
    failures += expect_scalar_sysdate(
        database,
        "SELECT SYSDATE() FROM DUAL",
        (struct expected_sysdate_scalar){.column = 0U, .offset_minutes = 0},
        (struct expected_query){
            .sql = NULL,
            .values = (const char *const[]){NULL},
            .column_count = 1U,
            .row_count = 1U,
            .context = "SYSDATE from dual",
        }
    );
    failures += expect_statement_ok(database, "SET time_zone = '+02:00'");
    failures += expect_scalar_sysdate(
        database,
        "SELECT SYSDATE()",
        (struct expected_sysdate_scalar){
            .column = 0U,
            .offset_minutes = sysdate_two_hour_offset_minutes,
        },
        (struct expected_query){
            .sql = NULL,
            .values = (const char *const[]){NULL},
            .column_count = 1U,
            .row_count = 1U,
            .context = "SYSDATE session time zone",
        }
    );
    failures += expect_statement_ok(database, "SET SESSION sql_mode = 'IGNORE_SPACE'");
    failures += expect_scalar_sysdate(
        database,
        "SELECT SYSDATE ()",
        (struct expected_sysdate_scalar){
            .column = 0U,
            .offset_minutes = sysdate_two_hour_offset_minutes,
        },
        (struct expected_query){
            .sql = NULL,
            .values = (const char *const[]){NULL},
            .column_count = 1U,
            .row_count = 1U,
            .context = "SYSDATE IGNORE_SPACE call",
        }
    );
    failures += expect_statement_ok(database, "DO SYSDATE()");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = do_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SYSDATE DO counts",
        }
    );
    mylite_close(database);
    return failures;
}

static int test_sysdate_dml_persistence_and_file_safety(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *independent = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "dml") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open dml file");
    failures += expect_int(mylite_open_memory(&independent), MYLITE_OK, "open independent memory");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE sysdate_values (id INT, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_statement_ok(database, "SET time_zone = '+00:00'");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures +=
        expect_dml_ok(database, "INSERT INTO sysdate_values VALUES (1, SYSDATE(), SYSDATE())", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO sysdate_values SET id = 2, dt = SYSDATE(), ts = SYSDATE()",
        1
    );
    failures +=
        expect_dml_ok(database, "REPLACE INTO sysdate_values VALUES (3, SYSDATE(), SYSDATE())", 1);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO sysdate_values SET id = 4, dt = SYSDATE(), ts = SYSDATE()",
        1
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO sysdate_values VALUES "
        "(5, '2001-01-01 00:00:00', '2001-01-01 00:00:00')",
        1
    );
    failures += expect_dml_ok(
        database,
        "UPDATE sysdate_values SET dt = SYSDATE(), ts = SYSDATE() WHERE id = 5",
        1
    );
    failures += expect_sysdate_table_rows(
        database,
        (struct expected_sysdate_table_query){
            .sql = "SELECT id, dt, ts FROM sysdate_values ORDER BY id",
            .context = "SYSDATE DML values",
        }
    );
    failures += expect_row_scalar_sysdate(database);
    failures += expect_statement_ok(independent, "SET time_zone = '+02:00'");
    failures += expect_scalar_sysdate(
        independent,
        "SELECT SYSDATE()",
        (struct expected_sysdate_scalar){
            .column = 0U,
            .offset_minutes = sysdate_two_hour_offset_minutes,
        },
        (struct expected_query){
            .sql = NULL,
            .values = (const char *const[]){NULL},
            .column_count = 1U,
            .row_count = 1U,
            .context = "independent handle SYSDATE",
        }
    );
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "SYSDATE leaves preamble"
    );

    mylite_close(independent);
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen dml file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_sysdate_table_rows(
        database,
        (struct expected_sysdate_table_query){
            .sql = "SELECT id, dt, ts FROM sysdate_values ORDER BY id",
            .context = "SYSDATE values persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sysdate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t (i INT, d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL, v VARCHAR(32))"
    );
    failures += expect_statement_ok(database, "INSERT INTO t(i) VALUES (1)");
    failures += execute_error(
        database,
        "INSERT INTO t(i) VALUES (SYSDATE())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SYSDATE values are supported only for DATETIME and TIMESTAMP columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t(d) VALUES (SYSDATE())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SYSDATE values are supported only for DATETIME and TIMESTAMP columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t(tm) VALUES (SYSDATE())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SYSDATE values are supported only for DATETIME and TIMESTAMP columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t(v) VALUES (SYSDATE())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SYSDATE values are supported only for DATETIME and TIMESTAMP columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET i = SYSDATE() WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SYSDATE values are supported only for DATETIME and TIMESTAMP columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSDATE(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SYSDATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSDATE ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSDATE (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSDATE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "dml affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "dml warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_scalar_sysdate(
    mylite_db *database,
    const char *sql,
    struct expected_sysdate_scalar expected,
    struct expected_query query
) {
    mylite_result *result = NULL;
    time_t before = time(NULL);
    time_t after = 0;
    int failures = execute_ok(database, sql, &result);

    after = time(NULL);
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            if (column == expected.column) {
                failures += expect_result_timestamp_near(
                    result,
                    row,
                    column,
                    (struct expected_sysdate_range){
                        .before = before,
                        .after = after,
                        .offset_minutes = expected.offset_minutes,
                    },
                    query.context
                );
            } else {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[value_index],
                    query.context
                );
            }
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_row_scalar_sysdate(mylite_db *database) {
    mylite_result *result = NULL;
    time_t before = time(NULL);
    time_t after = 0;
    int failures =
        execute_ok(database, "SELECT id, SYSDATE() FROM sysdate_values ORDER BY id", &result);

    after = time(NULL);
    failures += expect_size(mylite_result_column_count(result), 2U, "row-scalar column count");
    failures +=
        expect_size(mylite_result_row_count(result), sysdate_dml_row_count, "row-scalar row count");
    for (size_t row = 0U; row < sysdate_dml_row_count; ++row) {
        char expected_id[expected_id_capacity];
        int written = snprintf(expected_id, sizeof(expected_id), "%zu", row + 1U);

        if (written < 0 || (size_t)written >= sizeof(expected_id)) {
            failures += 1;
            continue;
        }
        failures += expect_result_value(result, row, 0U, expected_id, "row-scalar id");
        failures += expect_result_timestamp_near(
            result,
            row,
            1U,
            (struct expected_sysdate_range){
                .before = before,
                .after = after,
                .offset_minutes = 0,
            },
            "row-scalar SYSDATE value"
        );
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, "row-scalar affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "row-scalar warnings");
    mylite_result_free(result);

    return failures;
}

static int expect_sysdate_table_rows(
    mylite_db *database,
    struct expected_sysdate_table_query query
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 3U, query.context);
    failures += expect_size(mylite_result_row_count(result), sysdate_dml_row_count, query.context);
    for (size_t row = 0U; row < sysdate_dml_row_count; ++row) {
        char expected_id[expected_id_capacity];
        int written = snprintf(expected_id, sizeof(expected_id), "%zu", row + 1U);
        const char *datetime_value = mylite_result_value_text(result, row, 1U);
        const char *timestamp_value = mylite_result_value_text(result, row, 2U);

        if (written < 0 || (size_t)written >= sizeof(expected_id)) {
            failures += 1;
            continue;
        }
        failures += expect_result_value(result, row, 0U, expected_id, query.context);
        failures += expect_timestamp_shape(datetime_value, query.context);
        failures += expect_timestamp_shape(timestamp_value, query.context);
        failures += expect_true(
            datetime_value != NULL && strcmp(datetime_value, "2023-11-14 22:13:20") != 0,
            "SYSDATE DATETIME differs from SET timestamp"
        );
        failures += expect_true(
            timestamp_value != NULL && strcmp(timestamp_value, "2023-11-14 22:13:20") != 0,
            "SYSDATE TIMESTAMP differs from SET timestamp"
        );
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int expect_result_timestamp_near(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_sysdate_range expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (!timestamp_text_matches_range(actual, expected)) {
        fprintf(stderr, "%s: [%s] is not within the expected SYSDATE range\n", context, actual);
        return 1;
    }
    return 0;
}

static int timestamp_text_matches_range(
    const char *actual,
    struct expected_sysdate_range expected
) {
    int64_t start_epoch = (int64_t)expected.before - sysdate_range_tolerance_seconds;
    int64_t end_epoch = (int64_t)expected.after + sysdate_range_tolerance_seconds;

    if (!timestamp_shape_is_valid(actual)) {
        return 0;
    }
    for (int64_t epoch = start_epoch; epoch <= end_epoch; ++epoch) {
        char expected_text[timestamp_text_capacity];

        if (format_timestamp_for_epoch(
                epoch,
                expected.offset_minutes,
                expected_text,
                sizeof(expected_text)
            ) != 0) {
            return 0;
        }
        if (strcmp(actual, expected_text) == 0) {
            return 1;
        }
    }
    return 0;
}

static int format_timestamp_for_epoch(
    int64_t epoch,
    int offset_minutes,
    char *buffer,
    size_t buffer_size
) {
    int64_t adjusted_epoch = epoch + ((int64_t)offset_minutes * seconds_per_minute);
    time_t time_value = (time_t)adjusted_epoch;
    struct tm parts;
    int written = 0;

    if ((int64_t)time_value != adjusted_epoch) {
        return 1;
    }
#ifdef _WIN32
    if (gmtime_s(&parts, &time_value) != 0) {
        return 1;
    }
#else
    if (gmtime_r(&time_value, &parts) == NULL) {
        return 1;
    }
#endif
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        parts.tm_year + tm_year_calendar_offset,
        parts.tm_mon + 1,
        parts.tm_mday,
        parts.tm_hour,
        parts.tm_min,
        parts.tm_sec
    );
    return written < 0 || (size_t)written >= buffer_size;
}

static int expect_timestamp_shape(const char *actual, const char *context) {
    if (!timestamp_shape_is_valid(actual)) {
        fprintf(stderr, "%s: expected timestamp shape, got [%s]\n", context, actual);
        return 1;
    }
    return 0;
}

static int timestamp_shape_is_valid(const char *actual) {
    if (actual == NULL || strlen(actual) != timestamp_text_length) {
        return 0;
    }
    for (size_t index = 0U; index < timestamp_text_length; ++index) {
        if (!timestamp_character_is_valid(
                (struct timestamp_character_check){.value = actual[index], .index = index}
            )) {
            return 0;
        }
    }
    return 1;
}

static int timestamp_character_is_valid(struct timestamp_character_check check) {
    switch (check.index) {
    case timestamp_date_separator_1_index:
    case timestamp_date_separator_2_index:
        return check.value == '-';
    case timestamp_date_time_separator_index:
        return check.value == ' ';
    case timestamp_hour_separator_index:
    case timestamp_minute_separator_index:
        return check.value == ':';
    default:
        return is_ascii_digit(check.value);
    }
}

static int is_ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_sysdate_function_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        return 1;
    }
    return read_count == size ? 0 : 1;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
