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
    table_projection_row_count = 6,
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
    size_t warning_count;
    const char *context;
};

static int test_no_source_dual_and_do_from_unixtime(void);
static int test_table_backed_from_unixtime_and_reopen(void);
static int test_from_unixtime_ranges_and_diagnostics(void);
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

    failures += test_no_source_dual_and_do_from_unixtime();
    failures += test_table_backed_from_unixtime_and_reopen();
    failures += test_from_unixtime_ranges_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_from_unixtime(void) {
    static const char *const columns_core[] = {
        "epoch_zero",
        "epoch_one",
        "leap_day",
        "sample_dt",
        "null_dt",
        "true_dt",
        "false_dt",
        "warnings",
    };
    static const char *const values_core[] = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:00:01",
        "2000-02-29 00:00:00",
        "2015-11-13 16:08:01",
        NULL,
        "1970-01-01 00:00:01",
        "1970-01-01 00:00:00",
        "0",
    };
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_dual[] = {"dt"};
    static const char *const values_dual[] = {"1970-01-01 00:00:01"};
    static const char *const columns_timezone[] = {"epoch_zero", "epoch_one", "sample_dt"};
    static const char *const values_plus_offset[] = {
        "1970-01-01 02:30:00",
        "1970-01-01 02:30:01",
        "2015-11-13 18:38:01",
    };
    static const char *const values_minus_offset[] = {
        "1969-12-31 18:00:00",
        "1969-12-31 18:00:01",
        "2015-11-13 10:08:01",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_UNIXTIME(0) AS epoch_zero, "
                   "FROM_UNIXTIME(1) AS epoch_one, "
                   "FROM_UNIXTIME(951782400) AS leap_day, "
                   "FROM_UNIXTIME(1447430881) AS sample_dt, "
                   "FROM_UNIXTIME(NULL) AS null_dt, "
                   "FROM_UNIXTIME(TRUE) AS true_dt, "
                   "FROM_UNIXTIME(FALSE) AS false_dt, "
                   "@@warning_count AS warnings",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source from_unixtime core",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after from_unixtime select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_UNIXTIME (1) AS dt FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual from_unixtime",
        }
    );

    failures += execute_ok(database, "SET time_zone = '+02:30'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_UNIXTIME(0) AS epoch_zero, "
                   "FROM_UNIXTIME(1) AS epoch_one, "
                   "FROM_UNIXTIME(1447430881) AS sample_dt",
            .columns = columns_timezone,
            .column_count = sizeof(columns_timezone) / sizeof(columns_timezone[0]),
            .values = values_plus_offset,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "from_unixtime positive time zone",
        }
    );
    failures += execute_ok(database, "SET time_zone = '-06:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_UNIXTIME(0) AS epoch_zero, "
                   "FROM_UNIXTIME(1) AS epoch_one, "
                   "FROM_UNIXTIME(1447430881) AS sample_dt",
            .columns = columns_timezone,
            .column_count = sizeof(columns_timezone) / sizeof(columns_timezone[0]),
            .values = values_minus_offset,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "from_unixtime negative time zone",
        }
    );

    failures += execute_ok(database, "DO FROM_UNIXTIME(1), FROM_UNIXTIME(NULL)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "from_unixtime do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "from_unixtime do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "from_unixtime do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "from_unixtime do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after from_unixtime do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_from_unixtime_and_reopen(void) {
    static const char *const columns_table[] = {"id", "dt"};
    static const char *const values_table[] = {
        "1",
        "1970-01-01 00:00:00",
        "2",
        "1970-01-01 00:00:01",
        "3",
        "2000-02-29 00:00:00",
        "4",
        "2015-11-13 16:08:01",
        "5",
        NULL,
        "6",
        NULL,
    };
    static const char *const columns_limited[] = {"id", "dt"};
    static const char *const values_limited[] = {
        "3",
        "2000-02-29 00:00:00",
        "2",
        "1970-01-01 00:00:01",
    };
    static const char *const columns_reopen[] = {"dt"};
    static const char *const values_reopen[] = {"2000-02-29 00:00:00"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures +=
        execute_ok(database, "CREATE TABLE t(id INT, seconds BIGINT, label VARCHAR(16))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 0, 'zero'), (2, 1, 'one'), (3, 951782400, 'leap'), "
        "(4, 1447430881, 'sample'), (5, 32536771200, 'beyond'), (6, NULL, 'null')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FROM_UNIXTIME(seconds) AS dt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = table_projection_row_count,
            .warning_count = 0U,
            .context = "table from_unixtime projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FROM_UNIXTIME(seconds) AS dt FROM t "
                   "WHERE id BETWEEN 2 AND 3 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table from_unixtime where order limit",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen from_unixtime");
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_UNIXTIME(seconds) AS dt FROM t WHERE id = 3",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopened from_unixtime",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_from_unixtime_ranges_and_diagnostics(void) {
    static const char *const columns_range[] =
        {"negative", "positive", "maximum", "beyond", "warnings"};
    static const char *const values_range[] = {
        NULL,
        "1970-01-01 00:00:01",
        "3001-01-18 23:59:59",
        NULL,
        "0",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT, seconds BIGINT, v VARCHAR(16))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 1, '1')", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_UNIXTIME(-1) AS negative, "
                   "FROM_UNIXTIME(+1) AS positive, "
                   "FROM_UNIXTIME(32536771199) AS maximum, "
                   "FROM_UNIXTIME(32536771200) AS beyond, "
                   "@@warning_count AS warnings",
            .columns = columns_range,
            .column_count = sizeof(columns_range) / sizeof(columns_range[0]),
            .values = values_range,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "from_unixtime range behavior",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME() AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'FROM_UNIXTIME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME(1, 2, 3) AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'FROM_UNIXTIME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME(1, '%Y')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_UNIXTIME() format arguments are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME('1')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_UNIXTIME() supports only signed integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME(9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_UNIXTIME() integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME(v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_UNIXTIME() supports only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_UNIXTIME(seconds + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_UNIXTIME() supports only signed integer, boolean, NULL, "
                            "and integer descriptor arguments",
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
        "/tmp/mylite-from-unixtime-function-%s-%d.mylite",
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
