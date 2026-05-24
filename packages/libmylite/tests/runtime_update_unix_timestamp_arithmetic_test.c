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
    mysql_error_data_out_of_range = 1264,
    mysql_error_data_too_long = 1406,
    mysql_error_bigint_out_of_range = 1690,
    text_target_column_count = 6,
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

static int test_update_unix_timestamp_wordpress_text_persistence(void);
static int test_update_unix_timestamp_integer_and_limit_paths(void);
static int test_update_unix_timestamp_text_targets(void);
static int test_update_unix_timestamp_diagnostics(void);
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

    failures += test_update_unix_timestamp_wordpress_text_persistence();
    failures += test_update_unix_timestamp_integer_and_limit_paths();
    failures += test_update_unix_timestamp_text_targets();
    failures += test_update_unix_timestamp_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_update_unix_timestamp_wordpress_text_persistence(void) {
    static const char *const rows[] = {
        "_site_transient_timeout_tag1",
        "1704067110",
        "no",
        "other",
        "old",
        "yes",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "wp-text", path, sizeof(path));
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
        "INSERT INTO wp_options VALUES "
        "('_site_transient_timeout_tag1', 'old', 'no'), "
        "('other', 'old', 'yes')",
        NULL
    );
    failures += execute_ok(
        database,
        "UPDATE wp_options SET option_value = UNIX_TIMESTAMP() + -90 "
        "WHERE option_name = '_site_transient_timeout_tag1'",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "wp update unix timestamp text result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_name, option_value, autoload FROM wp_options ORDER BY "
                   "autoload",
            .values = rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "wp update unix timestamp text rows",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "update unix timestamp preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen wp update database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT option_name, option_value, autoload FROM wp_options ORDER BY "
                   "autoload",
            .values = rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "reopened wp update unix timestamp rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_update_unix_timestamp_integer_and_limit_paths(void) {
    static const char *const rows[] = {
        "1",
        "1704067201",
        "1704067199",
        NULL,
        "2",
        "1704067210",
        "0",
        "0",
        "3",
        "0",
        "0",
        "0",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "integers", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE events(id INT PRIMARY KEY, v BIGINT, u BIGINT UNSIGNED, n BIGINT NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO events VALUES (1, 0, 0, 0), (2, 0, 0, 0), (3, 0, 0, 0)",
        NULL
    );
    failures +=
        execute_ok(database, "UPDATE events SET v = UNIX_TIMESTAMP() + 1 WHERE id = 1", NULL);
    failures +=
        execute_ok(database, "UPDATE events SET u = UNIX_TIMESTAMP() - 1 WHERE id = 1", NULL);
    failures +=
        execute_ok(database, "UPDATE events SET n = UNIX_TIMESTAMP() + NULL WHERE id = 1", NULL);
    failures += execute_ok(
        database,
        "UPDATE events SET v = UNIX_TIMESTAMP() + 10 WHERE id IN (1, 2) ORDER BY id DESC "
        "LIMIT 1",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "ordered limited update unix timestamp result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "UPDATE events SET v = UNIX_TIMESTAMP() + 20 ORDER BY id LIMIT 0",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "limit zero update unix timestamp result",
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
            .context = "update unix timestamp integer rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_update_unix_timestamp_text_targets(void) {
    static const char *const text_rows[] = {
        "1704067200",
        "1704067110",
        "1704067205",
        "1704067207",
        NULL,
        "1704067208",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "text-targets", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE text_values("
        "c CHAR(12), v VARCHAR(12), txt TEXT, lt LONGTEXT, n VARCHAR(12), nn VARCHAR(12) NOT NULL"
        ")",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO text_values VALUES ('a','a','a','a','a','a')", NULL);
    failures += execute_ok(database, "UPDATE text_values SET c = UNIX_TIMESTAMP()", NULL);
    failures += execute_ok(database, "UPDATE text_values SET v = UNIX_TIMESTAMP() + -90", NULL);
    failures += execute_ok(database, "UPDATE text_values SET txt = UNIX_TIMESTAMP() - -5", NULL);
    failures += execute_ok(database, "UPDATE text_values SET lt = UNIX_TIMESTAMP() + 7", NULL);
    failures += execute_ok(database, "UPDATE text_values SET n = UNIX_TIMESTAMP() + NULL", NULL);
    failures += execute_ok(database, "UPDATE text_values SET nn = UNIX_TIMESTAMP() + 8", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c, v, txt, lt, n, nn FROM text_values",
            .values = text_rows,
            .column_count = text_target_column_count,
            .row_count = 1U,
            .context = "update unix timestamp text target rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_update_unix_timestamp_diagnostics(void) {
    static const char *const nonstrict_null[] = {""};
    static const char *const unchanged_short[] = {"x"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(database, "CREATE TABLE decimal_target(v DECIMAL(12,0))", NULL);
    failures += execute_ok(database, "INSERT INTO decimal_target VALUES (0)", NULL);
    failures += execute_error(
        database,
        "UPDATE decimal_target SET v = UNIX_TIMESTAMP()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UPDATE UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string "
                "targets",
        }
    );
    failures += execute_ok(database, "CREATE TABLE short_text(v VARCHAR(4))", NULL);
    failures += execute_ok(database, "INSERT INTO short_text VALUES ('x')", NULL);
    failures += execute_error(
        database,
        "UPDATE short_text SET v = UNIX_TIMESTAMP()",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += execute_ok(
        database,
        "UPDATE short_text SET v = UNIX_TIMESTAMP() WHERE v = 'missing'",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "no-match update unix timestamp skips conversion result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM short_text",
            .values = unchanged_short,
            .column_count = 1U,
            .row_count = 1U,
            .context = "no-match update unix timestamp leaves short text unchanged",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE generated_id(id BIGINT AUTO_INCREMENT PRIMARY KEY)",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO generated_id VALUES (NULL)", NULL);
    failures += execute_error(
        database,
        "UPDATE generated_id SET id = UNIX_TIMESTAMP()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not yet support AUTO_INCREMENT targets",
        }
    );
    failures += execute_ok(database, "CREATE TABLE required_text(v VARCHAR(12) NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO required_text VALUES ('x')", NULL);
    failures += execute_error(
        database,
        "UPDATE required_text SET v = UNIX_TIMESTAMP() + NULL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += execute_ok(database, "SET sql_mode = ''", NULL);
    failures +=
        execute_ok(database, "UPDATE required_text SET v = UNIX_TIMESTAMP() + NULL", &result);
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 1U,
            .context = "nonstrict update unix timestamp null text result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM required_text",
            .values = nonstrict_null,
            .column_count = 1U,
            .row_count = 1U,
            .context = "nonstrict update unix timestamp null text row",
        }
    );
    failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", NULL);
    failures += execute_ok(database, "CREATE TABLE small_target(v INT)", NULL);
    failures += execute_ok(database, "INSERT INTO small_target VALUES (0)", NULL);
    failures += execute_error(
        database,
        "UPDATE small_target SET v = UNIX_TIMESTAMP() + 1000000000",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'v' at row 1",
        }
    );
    failures += execute_ok(database, "CREATE TABLE overflow_target(v BIGINT)", NULL);
    failures += execute_ok(database, "INSERT INTO overflow_target VALUES (1)", NULL);
    failures += execute_error(
        database,
        "UPDATE overflow_target SET v = UNIX_TIMESTAMP() + 9223372036854775807",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_ok(
        database,
        "UPDATE overflow_target SET v = UNIX_TIMESTAMP() + 9223372036854775807 WHERE v = 999",
        &result
    );
    failures += expect_statement_result(
        result,
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "no-match update unix timestamp skips overflow result",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE overflow_target SET v = 1 + UNIX_TIMESTAMP()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax near 'UNIX_TIMESTAMP'",
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
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "execute '%s': expected error, got OK\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
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
    size_t value_index = 0U;

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_update_unix_timestamp_arithmetic_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
