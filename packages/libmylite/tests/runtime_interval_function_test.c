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
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    table_interval_row_count = 6,
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

static int test_no_source_dual_and_do_interval(void);
static int test_table_backed_interval(void);
static int test_interval_diagnostics(void);
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

    failures += test_no_source_dual_and_do_interval();
    failures += test_table_backed_interval();
    failures += test_interval_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_interval(void) {
    static const char *const columns_no_source[] = {
        "bucket",
        "zero_bucket",
        "edge_bucket",
        "null_bucket",
        "bool_bucket",
        "false_bucket",
        "duplicate_bucket",
        "min_bucket",
        "@@warning_count",
    };
    static const char *const values_no_source[] = {"3", "0", "3", "-1", "2", "1", "2", "1", "0"};
    static const char *const columns_dual[] = {"bucket", "max_bucket"};
    static const char *const values_dual[] = {"3", "2"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INTERVAL(23, 1, 15, 17, 30, 44, 200) AS bucket, "
                   "INTERVAL(0, 1, 15, 17) AS zero_bucket, "
                   "INTERVAL(200, 1, 15, 17) AS edge_bucket, "
                   "INTERVAL(NULL, 1, 2) AS null_bucket, "
                   "INTERVAL(TRUE, FALSE, TRUE, 2) AS bool_bucket, "
                   "INTERVAL(FALSE, FALSE, TRUE, 2) AS false_bucket, "
                   "INTERVAL(1, 1, 1, 2) AS duplicate_bucket, "
                   "INTERVAL(-9223372036854775808, -9223372036854775808, 0) AS min_bucket, "
                   "@@warning_count",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source interval",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INTERVAL (200, 1, 15, 17) AS bucket, "
                   "INTERVAL(9223372036854775807, 0, 9223372036854775807) AS max_bucket "
                   "FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual interval",
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
            .context = "row count after interval select",
        }
    );

    failures += execute_ok(database, "DO INTERVAL(1, 0, 1), INTERVAL(NULL, 1, 2)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "interval do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "interval do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "interval do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "interval do warnings");
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
            .context = "row count after interval do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_interval(void) {
    static const char *const columns_table[] = {"id", "bucket_a", "bucket_b"};
    static const char *const values_table[] = {
        "1",
        "-1",
        "-1",
        "2",
        "1",
        "0",
        "3",
        "2",
        "1",
        "4",
        "2",
        "1",
        "5",
        "3",
        "2",
        "6",
        "3",
        "3",
    };
    static const char *const columns_limited[] = {"id", "bucket"};
    static const char *const values_limited[] = {"6", "3", "5", "3", "4", "2"};
    static const char *const columns_integer_family[] = {
        "i_bucket",
        "j_bucket",
        "b_bucket",
        "u_bucket",
        "ub_bucket",
    };
    static const char *const values_integer_family[] = {"2", "2", "2", "2", "1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, n BIGINT NULL)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, NULL), (2, -5), (3, 0), (4, 1), (5, 10), (6, 100)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, INTERVAL(n, -5, 0, 10) AS bucket_a, "
                   "INTERVAL(n, 0, 10, 20) AS bucket_b FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = table_interval_row_count,
            .context = "table interval projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, INTERVAL(n, -5, 0, 10) AS bucket "
                   "FROM t WHERE id >= 2 ORDER BY id DESC LIMIT 3",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 3U,
            .context = "table interval where order limit",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE nums(i INT, j INTEGER, b BIGINT, u INT UNSIGNED, ub BIGINT UNSIGNED)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO nums VALUES (-1, 2, 9223372036854775807, 4, 9223372036854775807)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INTERVAL(i, -2, -1, 0) AS i_bucket, "
                   "INTERVAL(j, 1, 2, 3) AS j_bucket, "
                   "INTERVAL(b, 0, 9223372036854775807) AS b_bucket, "
                   "INTERVAL(u, 0, 4, 5) AS u_bucket, "
                   "INTERVAL(ub, 9223372036854775807) AS ub_bucket FROM nums",
            .columns = columns_integer_family,
            .column_count = sizeof(columns_integer_family) / sizeof(columns_integer_family[0]),
            .values = values_integer_family,
            .row_count = 1U,
            .context = "table interval integer families",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_interval_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, n INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 1)", NULL);
    failures += execute_error(
        database,
        "SELECT INTERVAL()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(missing, 1, 2) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(n, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() supports only literal threshold arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL('10', 1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() supports only integer, boolean, and NULL search arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(1, '10', 20)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() supports only integer and boolean threshold arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(1, NULL, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() threshold arguments cannot be NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(5, 10, 1, 20)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() threshold arguments must be sorted ascending",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(9223372036854775808, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(1, 9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT INTERVAL(n, 9223372036854775808) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INTERVAL() integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(INTERVAL(n, 1, 2), 'x') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT INTERVAL() is supported only as a top-level "
                            "projection expression",
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
        "/tmp/mylite-interval-function-%s-%d.mylite",
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
