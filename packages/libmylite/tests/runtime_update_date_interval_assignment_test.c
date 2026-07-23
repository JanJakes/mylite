#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_incorrect_datetime = 1292,
    mysql_error_datetime_overflow = 1441,
    date_interval_row_column_count = 6,
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

struct expected_statement_result {
    int64_t affected_rows;
    size_t warning_count;
    const char *context;
};

static int test_update_date_interval_success_and_persistence(void);
static int test_update_date_interval_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_result(
    mylite_result *result,
    struct expected_statement_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_update_date_interval_success_and_persistence();
    failures += test_update_date_interval_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_update_date_interval_success_and_persistence(void) {
    static const char *const rows[] = {
        "1",
        "2014-01-15 00:00:00",
        "2024-02-29",
        "2014-01-15 00:00:00",
        "2016-01-16",
        NULL,
        "2",
        "2017-03-01 00:00:00",
        "2023-01-31",
        "2017-02-28",
        "2017-02-28",
        NULL,
        "3",
        "2018-03-01 00:00:00",
        NULL,
        "2018-03-01",
        "2018-03-01",
        NULL,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "success", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE events("
        "id INT PRIMARY KEY, dt DATETIME NOT NULL, d DATE NULL, txt LONGTEXT NOT NULL, "
        "v VARCHAR(32) NOT NULL, nullable DATETIME NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO events VALUES "
        "(1,'2016-01-15 00:00:00','2024-01-31','2016-01-15 00:00:00',"
        "'2016-01-15','2016-01-15 00:00:00'), "
        "(2,'2017-02-28 00:00:00','2023-01-31','2017-02-28','2017-02-28',NULL), "
        "(3,'2018-03-01 00:00:00',NULL,'2018-03-01','2018-03-01',NULL)",
        NULL
    );

    failures += execute_ok(
        database,
        "UPDATE events SET dt = DATE_SUB(dt, INTERVAL '2' YEAR) WHERE id = 1",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "datetime date-sub update result",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "UPDATE events SET d = DATE_ADD(d, INTERVAL +1 MONTH) WHERE id = 1",
        NULL
    );
    failures += execute_ok(
        database,
        "UPDATE events SET txt = SUBDATE(txt, INTERVAL '+2' YEAR) WHERE id = 1",
        NULL
    );
    failures +=
        execute_ok(database, "UPDATE events SET v = ADDDATE(v, INTERVAL 1 DAY) WHERE id = 1", NULL);
    failures += execute_ok(
        database,
        "UPDATE events SET nullable = DATE_ADD(nullable, INTERVAL 1 DAY) WHERE id = 2",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "null source date interval update result",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "UPDATE events SET nullable = DATE_ADD(nullable, INTERVAL NULL DAY) WHERE id = 1",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "null interval date interval update result",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "UPDATE events SET dt = DATE_ADD(dt, INTERVAL 0 DAY) WHERE id = 2",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "zero interval no-op update result",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "UPDATE events SET dt = DATE_ADD(dt, INTERVAL 1 DAY) "
        "WHERE id IN (1, 2) ORDER BY id DESC LIMIT 1",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "ordered limited date interval update result",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "UPDATE events SET dt = DATE_ADD(dt, INTERVAL 5 DAY) ORDER BY id LIMIT 0",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "limit zero date interval update result",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt, d, txt, v, nullable FROM events ORDER BY id",
            .values = rows,
            .column_count = date_interval_row_column_count,
            .row_count = 3U,
            .context = "date interval updated rows",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "update date interval preserves preamble"
    );
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen update database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt, d, txt, v, nullable FROM events ORDER BY id",
            .values = rows,
            .column_count = date_interval_row_column_count,
            .row_count = 3U,
            .context = "reopened date interval rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_update_date_interval_diagnostics(void) {
    static const char *const invalid_rows[] = {"1", "bad"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE invalids(id INT PRIMARY KEY, v VARCHAR(32) NOT NULL)",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO invalids VALUES (1, 'bad')", NULL);
    failures += execute_ok(
        database,
        "UPDATE invalids SET v = DATE_ADD(v, INTERVAL 1 DAY) WHERE id = 2",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "no-match invalid date interval result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "UPDATE invalids SET v = DATE_ADD(v, INTERVAL 1 DAY) ORDER BY id LIMIT 0",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "limit zero invalid date interval result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM invalids",
            .values = invalid_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "invalid row unchanged",
        }
    );
    failures += execute_error(
        database,
        "UPDATE invalids SET v = DATE_ADD(v, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_datetime,
            .sqlstate = "22007",
            .message_part = "Incorrect datetime value: 'bad'",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE required(id INT PRIMARY KEY, dt DATETIME NOT NULL)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO required VALUES (1, '2016-01-15 00:00:00')", NULL);
    failures += execute_ok(
        database,
        "UPDATE required SET dt = DATE_ADD(dt, INTERVAL NULL DAY) WHERE id = 2",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "no-match null interval not null result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "UPDATE required SET dt = DATE_ADD(dt, INTERVAL NULL DAY) ORDER BY id LIMIT 0",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "limit zero null interval not null result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE required SET dt = DATE_ADD(dt, INTERVAL NULL DAY)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'dt' cannot be null",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE overflows(id INT PRIMARY KEY, dt DATETIME NULL)", NULL);
    failures +=
        execute_ok(database, "INSERT INTO overflows VALUES (1, '9999-12-31 23:59:59')", NULL);
    failures += execute_error(
        database,
        "UPDATE overflows SET dt = DATE_ADD(dt, INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_datetime_overflow,
            .sqlstate = "22008",
            .message_part = "Datetime function: datetime field overflow",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE unsupported(id INT PRIMARY KEY, i INT, d DATE, s VARCHAR(10), x VARCHAR(32) "
        "UNIQUE, ts TIMESTAMP NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO unsupported VALUES "
        "(1, 1, '2016-01-15', '2016-01-15', '2016-01-15', '2016-01-15 00:00:00')",
        NULL
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET i = DATE_ADD(i, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE DATE interval assignment supports only DATE",
        }
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET d = DATE_ADD(d, INTERVAL 1 HOUR)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not yet support time units for DATE targets",
        }
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET s = DATE_ADD(s, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE DATE interval assignment supports only DATE",
        }
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET x = DATE_ADD(x, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not yet support key columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET ts = DATE_ADD(ts, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE DATE interval assignment supports only DATE",
        }
    );
    failures += execute_error(
        database,
        "UPDATE IGNORE unsupported SET d = DATE_ADD(d, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE IGNORE does not yet support DATE interval assignments",
        }
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET d = DATE_ADD(i, INTERVAL 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports only the assigned column as input",
        }
    );
    failures += execute_error(
        database,
        "UPDATE unsupported SET d = DATE_ADD(d, INTERVAL 1 + 1 DAY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = mylite_test_make_path(path, path_size, name);

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "open test database");
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
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
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "execute '%s': expected error, got OK\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_result *result,
    struct expected_statement_result expected
) {
    int failures = 0;

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, expected.context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, expected.context);
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);
    size_t value_index = 0U;

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[value_index],
                    query.context
                );
                ++value_index;
            }
        }
    }

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
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s] at %zu,%zu, got [%s]\n",
            context,
            expected,
            row,
            column,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "failed to seek %s\n", path);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %s\n", path);
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
