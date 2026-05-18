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

static int test_no_source_and_dual_concat(void);
static int test_table_backed_concat(void);
static int test_table_backed_control_flow(void);
static int test_concat_diagnostics(void);
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

    failures += test_no_source_and_dual_concat();
    failures += test_table_backed_concat();
    failures += test_table_backed_control_flow();
    failures += test_concat_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_and_dual_concat(void) {
    static const char *const columns_no_source[] = {
        "DATABASE()",
        "SCHEMA()",
        "CONCAT('test-', DATABASE())",
        "CONCAT('a', 'b')",
        "CONCAT('solo')",
        "CONCAT(NULL)",
        "CONCAT('a', NULL)",
        "CONCAT(1, 2, 3)",
        "CONCAT(TRUE, FALSE)",
        "@@warning_count",
    };
    static const char *const values_no_source[] = {
        "app",
        "app",
        "test-app",
        "ab",
        "solo",
        NULL,
        NULL,
        "123",
        "10",
        "0",
    };
    static const char *const columns_dual[] = {"CONCAT('du', 'al')", "xy", "CONCAT('z')"};
    static const char *const values_dual[] = {"dual", "xy", "z"};
    static const char *const column_row_count[] = {"ROW_COUNT()"};
    static const char *const value_negative_one[] = {"-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE(), SCHEMA(), CONCAT('test-', DATABASE()), CONCAT('a', 'b'), "
                   "CONCAT('solo'), CONCAT(NULL), CONCAT('a', NULL), CONCAT(1, 2, 3), "
                   "CONCAT(TRUE, FALSE), @@warning_count",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT('du', 'al'), CONCAT('x', 'y') AS xy, CONCAT('z') FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual concat",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = column_row_count,
            .column_count = 1U,
            .values = value_negative_one,
            .row_count = 1U,
            .context = "row count after concat select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_concat(void) {
    static const char *const columns_mixed[] = {"id", "label"};
    static const char *const values_mixed[] = {"1", "a-7", "2", "b--3", "3", "-0"};
    static const char *const columns_nulls[] = {"id", "merged"};
    static const char *const values_nulls[] = {"1", "[ax]", "2", NULL, "3", "[]"};
    static const char *const columns_typed[] = {"id", "mixed"};
    static const char *const values_typed[] = {
        "1",
        "12.30:2024-01-02:01:02:03:2024-01-02 03:04:05:2024-01-02 03:04:05:alpha",
        "2",
        NULL,
        "3",
        NULL,
    };
    static const char *const columns_one_argument[] = {"id", "CONCAT(v)", "CONCAT('x')"};
    static const char *const values_one_argument[] = {"1", "a", "x"};
    static const char *const columns_limited[] = {"CONCAT(v, ':', id)"};
    static const char *const values_limited[] = {":3", "b:2"};
    static const char *const columns_labels[] = {"CONCAT(v, '-', id)", "alias_name"};
    static const char *const values_labels[] = {"a-1", "xapp"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(20), n VARCHAR(20), i INT, "
        "d DECIMAL(6,2), dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, txt TEXT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'a', 'x', 7, 12.30, '2024-01-02', '01:02:03', "
        "'2024-01-02 03:04:05', '2024-01-02 03:04:05', 'alpha'), "
        "(2, 'b', NULL, -3, -4.50, NULL, NULL, NULL, NULL, 'beta'), "
        "(3, '', '', 0, 0.00, '2024-12-31', '00:00:00', "
        "'2024-12-31 23:59:58', '2024-12-31 23:59:58', NULL)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(v, '-', i) AS label FROM t ORDER BY id",
            .columns = columns_mixed,
            .column_count = sizeof(columns_mixed) / sizeof(columns_mixed[0]),
            .values = values_mixed,
            .row_count = 3U,
            .context = "table concat projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT('[', v, n, ']') AS merged FROM t ORDER BY id",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 3U,
            .context = "table concat null propagation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(d, ':', dt, ':', tm, ':', dttm, ':', ts, ':', txt) "
                   "AS mixed FROM t ORDER BY id",
            .columns = columns_typed,
            .column_count = sizeof(columns_typed) / sizeof(columns_typed[0]),
            .values = values_typed,
            .row_count = 3U,
            .context = "table concat typed values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(v), CONCAT('x') FROM t WHERE id = 1",
            .columns = columns_one_argument,
            .column_count = sizeof(columns_one_argument) / sizeof(columns_one_argument[0]),
            .values = values_one_argument,
            .row_count = 1U,
            .context = "table concat one argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(v, ':', id) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table concat where order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(v, '-', id), CONCAT('x', DATABASE()) AS alias_name "
                   "FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "concat labels",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_control_flow(void) {
    static const char *const columns_string[] = {
        "id",
        "IFNULL(v,'x')",
        "COALESCE(n,v,'z')",
        "NULLIF(v,n)",
        "ISNULL(v)",
        "IF(nn, v, n)",
    };
    static const char *const values_string[] = {
        "1",
        "a",
        "a",
        "a",
        "0",
        "a",
        "2",
        "x",
        "fallback",
        NULL,
        "1",
        "fallback",
        "3",
        "A",
        "a",
        NULL,
        "0",
        "A",
    };
    static const char *const columns_integer[] = {
        "id",
        "IFNULL(i,-1)",
        "COALESCE(i,nn,99)",
        "NULLIF(i,0)",
        "ISNULL(i)",
        "IF(i, 'yes', 'no')",
    };
    static const char *const values_integer[] = {
        "1",
        "7",
        "7",
        "7",
        "0",
        "yes",
        "2",
        "-1",
        "0",
        NULL,
        "1",
        "no",
        "3",
        "0",
        "0",
        NULL,
        "0",
        "no",
    };
    static const char *const columns_temporal[] = {
        "id",
        "IFNULL(d,'2000-01-01')",
        "COALESCE(dt,'2000-01-01 00:00:00')",
        "IFNULL(txt,'missing')",
    };
    static const char *const values_temporal[] = {
        "1",
        "2024-01-02",
        "2024-01-02 03:04:05",
        "alpha",
        "2",
        "2000-01-01",
        "2000-01-01 00:00:00",
        "missing",
        "3",
        "2024-12-31",
        "2024-12-31 23:59:58",
        "beta",
    };
    static const char *const columns_limited[] = {"id", "IFNULL(v,'x')", "ISNULL(n)"};
    static const char *const values_limited[] = {"3", "A", "0", "2", "x", "0"};
    static const char *const columns_labels[] = {"IFNULL(v,'x')", "alias_name", "ISNULL(n)"};
    static const char *const values_labels[] = {"a", "a", "1"};
    static const char *const columns_qualified[] = {"id", "ifn"};
    static const char *const values_qualified[] = {"2", "x"};
    static const char *const columns_nested[] = {"id", "nested"};
    static const char *const values_nested[] = {"1", "a", "2", "fallback", "3", "a"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_status[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "control-flow", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(20), n VARCHAR(20), i INT, nn INT NOT NULL, "
        "d DATE, dt DATETIME, txt TEXT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'a', NULL, 7, 1, '2024-01-02', '2024-01-02 03:04:05', 'alpha'), "
        "(2, NULL, 'fallback', NULL, 0, NULL, NULL, NULL), "
        "(3, 'A', 'a', 0, 5, '2024-12-31', '2024-12-31 23:59:58', 'beta')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(v,'x'), COALESCE(n,v,'z'), NULLIF(v,n), "
                   "ISNULL(v), IF(nn, v, n) FROM t ORDER BY id",
            .columns = columns_string,
            .column_count = sizeof(columns_string) / sizeof(columns_string[0]),
            .values = values_string,
            .row_count = 3U,
            .context = "table control-flow string projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(i,-1), COALESCE(i,nn,99), NULLIF(i,0), "
                   "ISNULL(i), IF(i, 'yes', 'no') FROM t ORDER BY id",
            .columns = columns_integer,
            .column_count = sizeof(columns_integer) / sizeof(columns_integer[0]),
            .values = values_integer,
            .row_count = 3U,
            .context = "table control-flow integer projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(d,'2000-01-01'), "
                   "COALESCE(dt,'2000-01-01 00:00:00'), IFNULL(txt,'missing') "
                   "FROM t ORDER BY id",
            .columns = columns_temporal,
            .column_count = sizeof(columns_temporal) / sizeof(columns_temporal[0]),
            .values = values_temporal,
            .row_count = 3U,
            .context = "table control-flow temporal projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(v,'x'), ISNULL(n) FROM t "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table control-flow where order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(v,'x'), COALESCE(n,v) AS alias_name, ISNULL(n) "
                   "FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "control-flow labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT x.id, IFNULL(x.v,'x') AS ifn FROM t AS x WHERE x.id = 2",
            .columns = columns_qualified,
            .column_count = sizeof(columns_qualified) / sizeof(columns_qualified[0]),
            .values = values_qualified,
            .row_count = 1U,
            .context = "control-flow qualified columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IFNULL(NULLIF(v,n), COALESCE(n,'z')) AS nested "
                   "FROM t ORDER BY id",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 3U,
            .context = "control-flow nested projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_status,
            .row_count = 1U,
            .context = "control-flow status after select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_concat_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20), d DECIMAL(6,2))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a', 1.00)", NULL);
    failures += execute_error(
        database,
        "SELECT CONCAT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONCAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(v, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(CONCAT('a', 'b')) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT CONCAT() does not support nested CONCAT()",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(v + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT supports only CONCAT()",
        }
    );
    failures += execute_error(
        database,
        "SELECT IFNULL(v, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(v, 'yes', 'no') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IF() row conditions support only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT IFNULL(1 + 2, 3) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT supports only CONCAT()",
        }
    );
    failures += execute_error(
        database,
        "SELECT NULLIF(id, v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "NULLIF() row projection does not support mixed string and numeric",
        }
    );
    failures += execute_error(
        database,
        "SELECT NULLIF(d, '1.0') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT control-flow functions do not support DECIMAL "
                            "columns",
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
        "/tmp/mylite-row-scalar-%s-%d.mylite",
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
