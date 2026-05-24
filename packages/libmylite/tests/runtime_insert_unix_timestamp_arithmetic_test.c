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
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_data_out_of_range = 1264,
    mysql_error_data_too_long = 1406,
    mysql_error_bigint_out_of_range = 1690,
    text_values_column_count = 6,
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

static int test_insert_values_unix_timestamp_arithmetic_persistence(void);
static int test_insert_unix_timestamp_arithmetic_text_targets(void);
static int test_insert_set_replace_and_duplicate_update_unix_timestamp_arithmetic(void);
static int test_insert_unix_timestamp_arithmetic_diagnostics(void);
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
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_insert_values_unix_timestamp_arithmetic_persistence();
    failures += test_insert_unix_timestamp_arithmetic_text_targets();
    failures += test_insert_set_replace_and_duplicate_update_unix_timestamp_arithmetic();
    failures += test_insert_unix_timestamp_arithmetic_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_insert_values_unix_timestamp_arithmetic_persistence(void) {
    static const char *const rows[] = {
        "1",
        "1704067200",
        "1704067260",
        "1704067140",
        "2",
        "1704067201",
        "1704067201",
        NULL,
    };
    static const char *const last_insert_id[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "values", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE events("
        "id INT AUTO_INCREMENT PRIMARY KEY, v BIGINT, u BIGINT UNSIGNED, n BIGINT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO events(v, u, n) VALUES "
        "(UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + 60, UNIX_TIMESTAMP() - 60), "
        "(UNIX_TIMESTAMP() + +1, UNIX_TIMESTAMP() - -1, UNIX_TIMESTAMP() + NULL)",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 0U,
            .context = "insert values unix timestamp result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, u, n FROM events ORDER BY id",
            .values = rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "insert values unix timestamp rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "insert values unix timestamp last insert id",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "insert unix timestamp preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, u, n FROM events ORDER BY id",
            .values = rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "reopened insert values unix timestamp rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_unix_timestamp_arithmetic_text_targets(void) {
    static const char *const wp_rows[] = {
        "_site_transient_timeout_tag1",
        "1704067110",
        "no",
    };
    static const char *const text_rows[] = {
        "1704067200",
        "1704067110",
        "1704067205",
        "1704067207",
        NULL,
        "1704067208",
    };
    static const char *const text_event_rows[] = {
        "1",
        "1704067230",
        "1704067231",
        "2",
        "1704067220",
        NULL,
        "3",
        "1704067240",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "text-targets", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE wp_options("
        "option_name VARCHAR(191), option_value LONGTEXT, autoload VARCHAR(20)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO wp_options(option_name, option_value, autoload) "
        "VALUES ('_site_transient_timeout_tag1', UNIX_TIMESTAMP() + -90, 'no')",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "wp unix timestamp text insert result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_name, option_value, autoload FROM wp_options",
            .values = wp_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "wp unix timestamp text insert row",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE text_values("
        "v VARCHAR(32), txt TEXT, lt LONGTEXT, c CHAR(20), n VARCHAR(20), "
        "nn VARCHAR(20) NOT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO text_values(v, txt, lt, c, n, nn) VALUES "
        "(UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + -90, UNIX_TIMESTAMP() - -5, "
        "UNIX_TIMESTAMP() + 7, UNIX_TIMESTAMP() + NULL, UNIX_TIMESTAMP() + 8)",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v, txt, lt, c, n, nn FROM text_values",
            .values = text_rows,
            .column_count = text_values_column_count,
            .row_count = 1U,
            .context = "unix timestamp text-family target rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE text_events(id INT PRIMARY KEY, v LONGTEXT, s VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO text_events SET id = 1, v = UNIX_TIMESTAMP() + 10, "
        "s = UNIX_TIMESTAMP() + 11",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO text_events VALUES (2, 'old', 'old')", NULL);
    failures += execute_ok(
        database,
        "REPLACE INTO text_events(id, v, s) VALUES "
        "(2, UNIX_TIMESTAMP() + 20, UNIX_TIMESTAMP() + NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "REPLACE INTO text_events SET id = 3, v = UNIX_TIMESTAMP() + 40, "
        "s = UNIX_TIMESTAMP() + NULL",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO text_events(id, v, s) VALUES (1, 'old', 'old') "
        "ON DUPLICATE KEY UPDATE v = UNIX_TIMESTAMP() + 30, s = UNIX_TIMESTAMP() + 31",
        NULL
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, s FROM text_events ORDER BY id",
            .values = text_event_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "unix timestamp text set replace duplicate rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_set_replace_and_duplicate_update_unix_timestamp_arithmetic(void) {
    static const char *const rows[] = {
        "1",
        "1704067230",
        "1704067231",
        NULL,
        "2",
        "1704067220",
        "1704067221",
        NULL,
        "3",
        "1704067240",
        "1704067160",
        "1704067200",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "set-replace-duplicate", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE events(id INT PRIMARY KEY, v BIGINT, u BIGINT UNSIGNED, n BIGINT NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO events SET id = 1, v = UNIX_TIMESTAMP() + 10, "
        "u = UNIX_TIMESTAMP() - 10, n = UNIX_TIMESTAMP()",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "insert set unix timestamp result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO events(id, v, u, n) VALUES (2, 0, 0, NULL)", NULL);
    failures += execute_ok(
        database,
        "REPLACE INTO events(id, v, u, n) VALUES "
        "(2, UNIX_TIMESTAMP() + 20, UNIX_TIMESTAMP() + 21, UNIX_TIMESTAMP() + NULL)",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 0U,
            .context = "replace values unix timestamp result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "REPLACE INTO events SET id = 3, v = UNIX_TIMESTAMP() + 40, "
        "u = UNIX_TIMESTAMP() - 40, n = UNIX_TIMESTAMP()",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "replace set unix timestamp result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO events(id, v, u, n) VALUES (1, 0, 0, 0) "
        "ON DUPLICATE KEY UPDATE v = UNIX_TIMESTAMP() + 30, "
        "u = UNIX_TIMESTAMP() + 31, n = UNIX_TIMESTAMP() + NULL",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 0U,
            .context = "duplicate update unix timestamp result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, u, n FROM events ORDER BY id",
            .values = rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "set replace duplicate unix timestamp rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_unix_timestamp_arithmetic_diagnostics(void) {
    static const char *const ignored_null[] = {"0"};
    static const char *const ignored_text_null[] = {""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(database, "CREATE TABLE decimal_target(v DECIMAL(12,0))", NULL);
    failures += execute_error(
        database,
        "INSERT INTO decimal_target VALUES (UNIX_TIMESTAMP())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string "
                "targets",
        }
    );
    failures += execute_ok(database, "CREATE TABLE short_text(v VARCHAR(4))", NULL);
    failures += execute_error(
        database,
        "INSERT INTO short_text VALUES (UNIX_TIMESTAMP())",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE generated_id(id BIGINT AUTO_INCREMENT PRIMARY KEY)",
        NULL
    );
    failures += execute_error(
        database,
        "INSERT INTO generated_id(id) VALUES (UNIX_TIMESTAMP())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not yet support AUTO_INCREMENT targets",
        }
    );
    failures += execute_ok(database, "CREATE TABLE required_value(v BIGINT NOT NULL)", NULL);
    failures += execute_error(
        database,
        "INSERT INTO required_value VALUES (UNIX_TIMESTAMP() + NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += execute_ok(
        database,
        "INSERT IGNORE INTO required_value VALUES (UNIX_TIMESTAMP() + NULL)",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 1U,
            .context = "insert ignore unix timestamp null result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM required_value",
            .values = ignored_null,
            .column_count = 1U,
            .row_count = 1U,
            .context = "insert ignore unix timestamp null row",
        }
    );
    failures += execute_ok(database, "CREATE TABLE required_text(v VARCHAR(10) NOT NULL)", NULL);
    failures += execute_error(
        database,
        "INSERT INTO required_text VALUES (UNIX_TIMESTAMP() + NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += execute_ok(
        database,
        "INSERT IGNORE INTO required_text VALUES (UNIX_TIMESTAMP() + NULL)",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 1U,
            .context = "insert ignore unix timestamp text null result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM required_text",
            .values = ignored_text_null,
            .column_count = 1U,
            .row_count = 1U,
            .context = "insert ignore unix timestamp text null row",
        }
    );
    failures += execute_ok(database, "CREATE TABLE small_target(v INT)", NULL);
    failures += execute_error(
        database,
        "INSERT INTO small_target VALUES (UNIX_TIMESTAMP() + 1000000000)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'v' at row 1",
        }
    );
    failures += execute_ok(database, "CREATE TABLE overflow_target(v BIGINT)", NULL);
    failures += execute_error(
        database,
        "INSERT INTO overflow_target VALUES (UNIX_TIMESTAMP() + 9223372036854775807)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO overflow_target VALUES (1 + UNIX_TIMESTAMP())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
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
    int failures = make_test_path(path, path_size, name);

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open test database");
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_result *result,
    struct expected_statement_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 0U, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
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

    return expect_text(actual, expected, context);
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
        "%s/mylite_insert_unix_timestamp_arithmetic_%d_%s.mylite",
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
    remove_with_suffix(path, "-journal");
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

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }
    return 0;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
