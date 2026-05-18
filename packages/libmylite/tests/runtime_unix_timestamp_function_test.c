#include <mylite/mylite.h>

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
    path_suffix_capacity = 16,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_no_source_dual_and_do_unix_timestamp(void);
static int test_table_backed_unix_timestamp_and_reopen(void);
static int test_unix_timestamp_warnings_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_unix_timestamp();
    failures += test_table_backed_unix_timestamp_and_reopen();
    failures += test_unix_timestamp_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_unix_timestamp(void) {
    static const char *const columns_core[] = {
        "current_epoch",
        "null_epoch",
        "epoch_zero",
        "epoch_one",
        "before_epoch",
        "max_epoch",
        "beyond_epoch",
    };
    static const char *const values_core[] = {
        "1704067200",
        NULL,
        "0",
        "1",
        "0",
        "32536771199",
        "0",
    };
    static const char *const columns_offset[] = {
        "current_epoch",
        "date_epoch",
        "datetime_epoch",
        "clipped_epoch",
    };
    static const char *const values_offset[] = {
        "1704067200",
        "1704063600",
        "1704063600",
        "0",
    };
    static const char *const columns_dual[] = {"epoch"};
    static const char *const values_dual[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET timestamp = 1704067200", NULL);
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNIX_TIMESTAMP() AS current_epoch, "
                   "UNIX_TIMESTAMP(NULL) AS null_epoch, "
                   "UNIX_TIMESTAMP('1970-01-01 00:00:00') AS epoch_zero, "
                   "UNIX_TIMESTAMP('1970-01-01 00:00:01') AS epoch_one, "
                   "UNIX_TIMESTAMP('1969-12-31 23:59:59') AS before_epoch, "
                   "UNIX_TIMESTAMP('3001-01-18 23:59:59') AS max_epoch, "
                   "UNIX_TIMESTAMP('3001-01-19 03:14:07') AS beyond_epoch",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source unix_timestamp core",
        }
    );

    failures += execute_ok(database, "SET time_zone = '+01:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNIX_TIMESTAMP() AS current_epoch, "
                   "UNIX_TIMESTAMP('2024-01-01') AS date_epoch, "
                   "UNIX_TIMESTAMP('2024-01-01 00:00:00') AS datetime_epoch, "
                   "UNIX_TIMESTAMP('1970-01-01 00:00:01') AS clipped_epoch",
            .columns = columns_offset,
            .column_count = sizeof(columns_offset) / sizeof(columns_offset[0]),
            .values = values_offset,
            .row_count = 1U,
            .context = "no-source unix_timestamp time zone",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNIX_TIMESTAMP ('1970-01-01 01:00:01') AS epoch FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual unix_timestamp",
        }
    );

    failures += execute_ok(database, "DO UNIX_TIMESTAMP(), UNIX_TIMESTAMP(NULL)", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 0U, "unix_timestamp do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "unix_timestamp do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "unix_timestamp do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "unix_timestamp do warnings");
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_unix_timestamp_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "date_epoch",
        "datetime_epoch",
        "timestamp_epoch",
    };
    static const char *const values_table[] = {
        "1",
        "82800",
        "0",
        "1",
        "2",
        "1704063600",
        "1704063600",
        "1704067200",
        "3",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"dt_epoch"};
    static const char *const values_limited[] = {NULL, "1704063600"};
    static const char *const columns_reopen[] = {"dt_epoch", "ts_epoch"};
    static const char *const values_reopen[] = {"1704067200", "1704067200"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE events("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, label VARCHAR(32)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO events VALUES "
        "(1, '1970-01-02', '1970-01-01 00:00:01', '1970-01-01 00:00:01', 'one'), "
        "(2, '2024-01-01', '2024-01-01 00:00:00', '2024-01-01 00:00:00', 'two'), "
        "(3, NULL, NULL, NULL, NULL)",
        NULL
    );

    failures += execute_ok(database, "SET time_zone = '+01:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, UNIX_TIMESTAMP(d) AS date_epoch, "
                   "UNIX_TIMESTAMP(dt) AS datetime_epoch, "
                   "UNIX_TIMESTAMP(ts) AS timestamp_epoch "
                   "FROM events ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table unix_timestamp projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNIX_TIMESTAMP(dt) AS dt_epoch FROM events "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table unix_timestamp where order limit",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen unix_timestamp");
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNIX_TIMESTAMP(dt) AS dt_epoch, UNIX_TIMESTAMP(ts) AS ts_epoch "
                   "FROM events WHERE id = 2",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopened unix_timestamp",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_unix_timestamp_warnings_and_diagnostics(void) {
    static const char *const columns_row_warning_query[] = {"id", "bad", "zero"};
    static const char *const values_row_warning_query[] = {
        "1",
        "0.000000",
        "0",
        "2",
        "0.000000",
        "0",
    };
    static const char *const values_row_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, d DATE NULL, v VARCHAR(32), dt DATETIME NULL)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (1, '2024-01-01', '2024-01-01', NULL)", NULL);
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (2, '2024-01-02', '2024-01-02', NULL)", NULL);

    failures += execute_ok(
        database,
        "SELECT id, UNIX_TIMESTAMP('bad') AS bad, "
        "UNIX_TIMESTAMP('0000-00-00') AS zero FROM t ORDER BY id",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 3U, "row warning columns");
        failures += expect_size(mylite_result_row_count(result), 2U, "row warning rows");
        failures += expect_size(mylite_result_warning_count(result), 4U, "row warnings");
        for (size_t column = 0U; column < 3U; ++column) {
            failures += expect_text(
                mylite_result_column_name(result, column),
                columns_row_warning_query[column],
                "row warning column name"
            );
        }
        for (size_t row = 0U; row < 2U; ++row) {
            for (size_t column = 0U; column < 3U; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    values_row_warning_query[(row * 3U) + column],
                    "row warning value"
                );
            }
        }
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_row_warnings,
            .row_count = 4U,
            .context = "row unix_timestamp warnings",
        }
    );

    failures += execute_ok(
        database,
        "SELECT UNIX_TIMESTAMP('bad') AS bad, UNIX_TIMESTAMP('0000-00-00') AS zero, "
        "UNIX_TIMESTAMP('2024-00-01') AS partial",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 3U, "invalid columns");
        failures += expect_size(mylite_result_row_count(result), 1U, "invalid rows");
        failures += expect_result_value(result, 0U, 0U, "0.000000", "invalid bad value");
        failures += expect_result_value(result, 0U, 1U, "0", "invalid zero value");
        failures += expect_result_value(result, 0U, 2U, "0", "invalid partial value");
        failures += expect_size(mylite_result_warning_count(result), 2U, "invalid warnings");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_warnings,
            .row_count = 2U,
            .context = "unix_timestamp warnings",
        }
    );

    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP('1970-01-01', 'extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'UNIX_TIMESTAMP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNIX_TIMESTAMP() supports only string temporal literals, DATE, "
                            "DATETIME, TIMESTAMP descriptor columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP('2024-01-01 00:00:00.1')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNIX_TIMESTAMP() does not yet support fractional seconds",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP(v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNIX_TIMESTAMP() does not yet support string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP(id) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNIX_TIMESTAMP() supports only DATE, DATETIME, TIMESTAMP "
                            "descriptor columns, string temporal literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNIX_TIMESTAMP(DATE_ADD(dt, INTERVAL 1 SECOND)) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNIX_TIMESTAMP() supports only string temporal literals, DATE, "
                            "DATETIME, TIMESTAMP descriptor columns, and NULL",
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
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-unix-timestamp-function-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
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
