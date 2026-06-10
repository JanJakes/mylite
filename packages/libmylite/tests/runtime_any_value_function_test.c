#include <mylite/mylite.h>

#include <stdint.h>
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
    any_value_scalar_column_count = 5,
    any_value_spaced_column_count = 1,
    any_value_row_scalar_column_count = 1,
    any_value_fixture_row_count = 5,
    any_value_grouped_column_count = 5,
    any_value_grouped_having_column_count = 3,
    any_value_grouped_order_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_native_function_arity = 1582,
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
    int64_t affected_rows;
    const char *context;
};

static int test_any_value_scalar_values(void);
static int test_any_value_row_scalar_and_grouped_values(void);
static int test_any_value_errors_and_identifier_use(void);
static int create_any_value_fixture(mylite_db *database);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_any_value_scalar_values();
    failures += test_any_value_row_scalar_and_grouped_values();
    failures += test_any_value_errors_and_identifier_use();

    return failures == 0 ? 0 : 1;
}

static int test_any_value_scalar_values(void) {
    static const char *const scalar_columns[] = {
        "ANY_VALUE(1)",
        "ANY_VALUE(NULL)",
        "ANY_VALUE('abc')",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const scalar_values[] = {"1", NULL, "abc", "0", "0"};
    static const char *const spaced_columns[] = {"ANY_VALUE (1)"};
    static const char *const spaced_values[] = {"1"};
    static const char *const from_dual_columns[] = {"ANY_VALUE('dual')"};
    static const char *const from_dual_values[] = {"dual"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar any_value");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ANY_VALUE(1),ANY_VALUE(NULL),ANY_VALUE('abc'),@@warning_count,"
                   "ROW_COUNT()",
            .columns = scalar_columns,
            .column_count = any_value_scalar_column_count,
            .values = scalar_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar any_value values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ANY_VALUE (1)",
            .columns = spaced_columns,
            .column_count = any_value_spaced_column_count,
            .values = spaced_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "spaced any_value value",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ANY_VALUE('dual') FROM DUAL",
            .columns = from_dual_columns,
            .column_count = any_value_spaced_column_count,
            .values = from_dual_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "from dual any_value value",
        }
    );

    failures += execute_ok(database, "DO ANY_VALUE(1), ANY_VALUE(NULL)", &result);
    failures += expect_size(mylite_result_column_count(result), 0U, "any_value DO columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "any_value DO rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "any_value DO warnings");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "any_value DO affected rows");
    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_any_value_row_scalar_and_grouped_values(void) {
    static const char *const row_scalar_columns[] = {"ANY_VALUE(v)"};
    static const char *const row_scalar_values[] = {"10", "10", NULL, NULL, "20"};
    static const char *const grouped_columns[] = {
        "g",
        "ANY_VALUE(v)",
        "ANY_VALUE(s)",
        "MAX(v)",
        "COUNT(*)",
    };
    static const char *const grouped_values[] = {
        "1",
        "10",
        "ten",
        "10",
        "2",
        "2",
        NULL,
        NULL,
        NULL,
        "2",
        "3",
        "20",
        "twenty",
        "20",
        "1",
    };
    static const char *const grouped_having_columns[] = {"g", "av", "mx"};
    static const char *const grouped_having_values[] = {"1", "10", "10", "3", "20", "20"};
    static const char *const grouped_order_columns[] = {"g", "av"};
    static const char *const grouped_order_values[] = {"3", "20", "1", "10", "2", NULL};
    static const char *const grouped_limit_values[] = {"3", "20", "1", "10"};
    static const char *const grouped_qualified_values[] = {"1", "10", "2", NULL, "3", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "grouped") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open grouped any_value");
    failures += create_any_value_fixture(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ANY_VALUE(v) FROM t ORDER BY g, v",
            .columns = row_scalar_columns,
            .column_count = any_value_row_scalar_column_count,
            .values = row_scalar_values,
            .row_count = any_value_fixture_row_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row scalar any_value values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ANY_VALUE(v) FROM t WHERE g = 99",
            .columns = row_scalar_columns,
            .column_count = any_value_row_scalar_column_count,
            .values = row_scalar_values,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row scalar any_value empty filter",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, ANY_VALUE(v), ANY_VALUE(s), MAX(v), COUNT(*) FROM t GROUP BY g "
                   "ORDER BY g",
            .columns = grouped_columns,
            .column_count = any_value_grouped_column_count,
            .values = grouped_values,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped any_value values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, ANY_VALUE(v) AS av, MAX(v) AS mx FROM t GROUP BY g "
                   "HAVING av IS NOT NULL ORDER BY g",
            .columns = grouped_having_columns,
            .column_count = any_value_grouped_having_column_count,
            .values = grouped_having_values,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped any_value having alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, ANY_VALUE(v) AS av FROM t GROUP BY g ORDER BY av DESC",
            .columns = grouped_order_columns,
            .column_count = any_value_grouped_order_column_count,
            .values = grouped_order_values,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped any_value order alias",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, ANY_VALUE(v) AS av FROM t GROUP BY g ORDER BY av DESC LIMIT 2",
            .columns = grouped_order_columns,
            .column_count = any_value_grouped_order_column_count,
            .values = grouped_limit_values,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped any_value order alias limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, ANY_VALUE(t.v) AS av FROM t GROUP BY g ORDER BY g",
            .columns = grouped_order_columns,
            .column_count = any_value_grouped_order_column_count,
            .values = grouped_qualified_values,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "grouped qualified any_value argument",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_any_value_errors_and_identifier_use(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open any_value errors");
    failures += create_any_value_fixture(database);
    failures += execute_ok(database, "CREATE TABLE any_value(id INT)", NULL);
    failures += execute_ok(database, "DROP TABLE any_value", NULL);
    failures += execute_error(
        database,
        "SELECT ANY_VALUE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ANY_VALUE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ANY_VALUE(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ANY_VALUE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ANY_VALUE(*) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT ANY_VALUE(DISTINCT v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT ANY_VALUE(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT g, ANY_VALUE(v + 1) FROM t GROUP BY g",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ANY_VALUE(column) supports only descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT ANY_VALUE(v), MAX(v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MIN/MAX supports exactly one aggregate select item",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int create_any_value_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(g INT, v INT, s VARCHAR(20))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1,10,'ten'),(1,10,'ten'),(2,NULL,NULL),(2,NULL,NULL),"
        "(3,20,'twenty')",
        NULL
    );
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *tmpdir = getenv("TMPDIR");
    int written = 0;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_any_value_function_%d_%s.mylite",
        tmpdir,
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
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
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
