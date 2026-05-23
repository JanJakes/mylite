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
    path_suffix_capacity = 16,
    timestampadd_core_column_count = 8,
    timestampadd_table_column_count = 5,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
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
    size_t warning_count;
    const char *context;
};

static int test_no_source_dual_and_do_timestampadd(void);
static int test_table_backed_timestampadd_and_reopen(void);
static int test_timestampadd_diagnostics_and_independent_handles(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_no_source_dual_and_do_timestampadd();
    failures += test_table_backed_timestampadd_and_reopen();
    failures += test_timestampadd_diagnostics_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_timestampadd(void) {
    static const char *const core_columns[] = {
        "TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17')",
        "TIMESTAMPADD(SQL_TSI_SECOND, +1, \"2008-01-02 13:29:17\")",
        "TIMESTAMPADD(SECOND, -1, '2008-01-02 13:29:17')",
        "TIMESTAMPADD(SECOND, 0, '2008-01-02 13:29:17')",
        "TIMESTAMPADD(SECOND, 1, '2008-01-02')",
        "TIMESTAMPADD(SECOND, NULL, '2008-01-02 13:29:17')",
        "TIMESTAMPADD(SECOND, 1, NULL)",
        "@@warning_count",
    };
    static const char *const core_values[] = {
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:17",
        "2008-01-02 00:00:01",
        NULL,
        NULL,
        "0",
    };
    static const char *const label_columns[] = {
        "TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17')",
        "shifted",
    };
    static const char *const label_values[] = {
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
    };
    static const char *const rollover_columns[] = {
        "TIMESTAMPADD(SECOND, 1, '2024-02-28 23:59:59')",
        "TIMESTAMPADD(SECOND, 1, '2024-02-29 23:59:59')",
    };
    static const char *const rollover_values[] = {
        "2024-02-29 00:00:00",
        "2024-03-01 00:00:00",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const row_count_values[] = {"0", "0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open scalar database");
    if (failures != 0) {
        return failures;
    }
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17'), "
                   "TIMESTAMPADD(SQL_TSI_SECOND, +1, \"2008-01-02 13:29:17\"), "
                   "TIMESTAMPADD(SECOND, -1, '2008-01-02 13:29:17'), "
                   "TIMESTAMPADD(SECOND, 0, '2008-01-02 13:29:17'), "
                   "TIMESTAMPADD(SECOND, 1, '2008-01-02'), "
                   "TIMESTAMPADD(SECOND, NULL, '2008-01-02 13:29:17'), "
                   "TIMESTAMPADD(SECOND, 1, NULL), @@warning_count",
            .columns = core_columns,
            .column_count = timestampadd_core_column_count,
            .values = core_values,
            .row_count = 1U,
            .context = "core timestampadd values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17'), "
                   "TIMESTAMPADD(SECOND, -1, '2008-01-02 13:29:17') AS shifted FROM DUAL",
            .columns = label_columns,
            .column_count = sizeof(label_columns) / sizeof(label_columns[0]),
            .values = label_values,
            .row_count = 1U,
            .context = "timestampadd labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPADD(SECOND, 1, '2024-02-28 23:59:59'), "
                   "TIMESTAMPADD(SECOND, 1, '2024-02-29 23:59:59')",
            .columns = rollover_columns,
            .column_count = sizeof(rollover_columns) / sizeof(rollover_columns[0]),
            .values = rollover_values,
            .row_count = 1U,
            .context = "timestampadd leap rollover",
        }
    );
    failures += execute_ok(
        database,
        "DO TIMESTAMPADD(SECOND, 1, '2008-01-02 13:29:17'), TIMESTAMPADD(SECOND, 1, NULL)",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "timestampadd do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "timestampadd do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "timestampadd do rows");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "timestampadd do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_count_columns,
            .column_count = sizeof(row_count_columns) / sizeof(row_count_columns[0]),
            .values = row_count_values,
            .row_count = 1U,
            .context = "timestampadd row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPADD (SECOND, 1, '2008-01-02 13:29:17') AS shifted",
            .columns = (const char *const[]){"shifted"},
            .column_count = 1U,
            .values = (const char *const[]){"2008-01-02 13:29:18"},
            .row_count = 1U,
            .context = "timestampadd whitespace",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_timestampadd_and_reopen(void) {
    static const char *const table_columns[] = {"id", "d1", "dt1", "ts1", "v1"};
    static const char *const table_values[] = {
        "1",
        "2008-01-02 00:00:01",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:19",
        "2008-01-02 13:29:18",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const warning_columns[] = {"id", "shifted_dt", "shifted_v"};
    static const char *const warning_values[] = {"3", NULL, NULL};
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const show_warning_values[] = {
        "Warning",
        "1441",
        "Datetime function: datetime field overflow",
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
    };
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (open_app_database(&database, "table", path, sizeof(path)) != 0) {
        return 1;
    }
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL, v VARCHAR(32), tm TIME)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '2008-01-02', '2008-01-02 13:29:17', '2008-01-02 13:29:17', "
        "'2008-01-02 13:29:17', '01:02:03'), "
        "(2, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIMESTAMPADD(SECOND, 1, d) AS d1, "
                   "TIMESTAMPADD(SECOND, -1, dt) AS dt1, "
                   "TIMESTAMPADD(SQL_TSI_SECOND, 2, ts) AS ts1, "
                   "TIMESTAMPADD(SECOND, 1, v) AS v1 FROM t WHERE id < 3 ORDER BY id",
            .columns = table_columns,
            .column_count = timestampadd_table_column_count,
            .values = table_values,
            .row_count = 2U,
            .context = "table timestampadd projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPADD() does not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, id, dt) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TIMESTAMPADD() INTERVAL SECOND supports only signed integer literals and NULL",
        }
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (3, NULL, '9999-12-31 23:59:59', NULL, 'bad', NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIMESTAMPADD(SECOND, 1, dt) AS shifted_dt, "
                   "TIMESTAMPADD(SECOND, 1, v) AS shifted_v FROM t WHERE id = 3",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "table timestampadd invalid warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = sizeof(show_warning_columns) / sizeof(show_warning_columns[0]),
            .values = show_warning_values,
            .row_count = 2U,
            .context = "table timestampadd warning details",
        }
    );
    mylite_file_preamble_init(expected_preamble);
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read timestampadd preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "timestampadd preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen timestampadd file");
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIMESTAMPADD(SECOND, 1, d) AS d1, "
                   "TIMESTAMPADD(SECOND, -1, dt) AS dt1, "
                   "TIMESTAMPADD(SQL_TSI_SECOND, 2, ts) AS ts1, "
                   "TIMESTAMPADD(SECOND, 1, v) AS v1 FROM t WHERE id < 3 ORDER BY id",
            .columns = table_columns,
            .column_count = timestampadd_table_column_count,
            .values = table_values,
            .row_count = 2U,
            .context = "reopened table timestampadd projection",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_timestampadd_diagnostics_and_independent_handles(void) {
    static const char *const first_columns[] = {"shifted"};
    static const char *const first_values[] = {"2010-01-01 00:00:01"};
    static const char *const second_values[] = {"2020-01-01 00:00:01"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics db");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(MINUTE, 1, '2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPADD() supports only SECOND and SQL_TSI_SECOND units",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(MICROSECOND, 1, '2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPADD() supports only SECOND and SQL_TSI_SECOND units",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1 + 1, '2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TIMESTAMPADD() INTERVAL SECOND supports only signed integer literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, '1', '2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TIMESTAMPADD() INTERVAL SECOND supports only signed integer literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 9223372036854775808, '2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TIMESTAMPADD() INTERVAL SECOND literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TIMESTAMPADD() supports only date or datetime string literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, '2016-07-00')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TIMESTAMPADD() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, '9999-12-31 23:59:59')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPADD() result is outside the supported datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    mylite_close(database);

    if (open_app_database(&first, "first", first_path, sizeof(first_path)) != 0 ||
        open_app_database(&second, "second", second_path, sizeof(second_path)) != 0) {
        mylite_close(first);
        mylite_close(second);
        return failures + 1;
    }
    failures += execute_ok(first, "CREATE TABLE t (v DATETIME)", NULL);
    failures += execute_ok(second, "CREATE TABLE t (v DATETIME)", NULL);
    failures += execute_ok(first, "INSERT INTO t VALUES ('2010-01-01 00:00:00')", NULL);
    failures += execute_ok(second, "INSERT INTO t VALUES ('2020-01-01 00:00:00')", NULL);
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPADD(SECOND, 1, v) AS shifted FROM t",
            .columns = first_columns,
            .column_count = sizeof(first_columns) / sizeof(first_columns[0]),
            .values = first_values,
            .row_count = 1U,
            .context = "first handle timestampadd",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPADD(SECOND, 1, v) AS shifted FROM t",
            .columns = first_columns,
            .column_count = sizeof(first_columns) / sizeof(first_columns[0]),
            .values = second_values,
            .row_count = 1U,
            .context = "second handle timestampadd",
        }
    );
    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
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
    if (failures == 0) {
        failures += execute_ok(*out_database, "SET sql_mode = ''", NULL);
    }
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
    failures += expect_int(mylite_errcode(database), expected.code, "execute error");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "execute error");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "execute error");
    mylite_result_free(result);
    return failures;
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-timestampadd-second-%s-%d.mylite",
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fclose(file);
        fprintf(stderr, "%s: failed to read file\n", path);
        return 1;
    }
    fclose(file);
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
